using System.ComponentModel;
using System.Globalization;
using System.Runtime.ExceptionServices;
using System.Runtime.InteropServices;
using System.Text.Json;
using Microsoft.Win32.SafeHandles;
using RuntimeLibrary = System.Runtime.InteropServices.NativeLibrary;

namespace LiveKit.Server;

/// <summary>Owns a shared C ABI library. In-flight calls retain a module lease through cleanup.</summary>
public sealed class NativeServer : IDisposable
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int VersionFunction();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void FreeFunction(nint output);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int Callback(nint user, nint request, nuint length,
                                  out nint response, out nuint responseLength);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate nint InvokeFunction(nint input, nuint length, Callback callback,
                                         nint user, out nuint outputLength);

    private sealed class LibraryHandle : SafeHandleZeroOrMinusOneIsInvalid
    {
        internal LibraryHandle() : base(true) { }
        internal void SetModule(nint module) => SetHandle(module);
        protected override bool ReleaseHandle()
        {
            RuntimeLibrary.Free(handle);
            return true;
        }
    }

    private readonly LibraryHandle _library = new();
    private readonly InvokeFunction _invoke;
    private readonly FreeFunction _free;
    private static readonly Lazy<NativeServer> Shared = new(() => new NativeServer());
    internal static NativeServer Default => Shared.Value;
    public int AbiVersion => 1;

    public NativeServer(string? path = null, IEnumerable<string>? dllDirectories = null)
    {
        path ??= Environment.GetEnvironmentVariable("LIVEKIT_SERVER_DOTNET_LIBRARY");
        var explicitPath = !string.IsNullOrEmpty(path);
        path = explicitPath ? Path.GetFullPath(path!) : (OperatingSystem.IsWindows()
            ? "livekitserver.dll" : OperatingSystem.IsMacOS() ? "liblivekitserver.dylib" : "liblivekitserver.so");
        var cookies = new List<nint>();
        nint module;
        try
        {
            if (OperatingSystem.IsWindows())
            {
                var directories = dllDirectories ?? (Environment.GetEnvironmentVariable(
                    "LIVEKIT_SERVER_DOTNET_DLL_DIRECTORIES") ?? "").Split(Path.PathSeparator,
                        StringSplitOptions.RemoveEmptyEntries);
                foreach (var directory in directories)
                {
                    var cookie = AddDllDirectory(Path.GetFullPath(directory));
                    if (cookie == 0) throw new Win32Exception(Marshal.GetLastWin32Error());
                    try { cookies.Add(cookie); }
                    catch { RemoveDllDirectory(cookie); throw; }
                }
            }
            // Explicit DLL paths also search their parent for native dependency DLLs.
            module = RuntimeLibrary.Load(path, typeof(NativeServer).Assembly,
                DllImportSearchPath.UseDllDirectoryForDependencies | DllImportSearchPath.SafeDirectories);
            _library.SetModule(module);
        }
        finally
        {
            foreach (var cookie in cookies) RemoveDllDirectory(cookie);
        }
        try
        {
            var version = Marshal.GetDelegateForFunctionPointer<VersionFunction>(
                RuntimeLibrary.GetExport(module, "lks_abi_version"));
            if (version() != 1) throw new NotSupportedException("unsupported LiveKit server C ABI version (expected 1)");
            _free = Marshal.GetDelegateForFunctionPointer<FreeFunction>(RuntimeLibrary.GetExport(module, "lks_free"));
            _invoke = Marshal.GetDelegateForFunctionPointer<InvokeFunction>(RuntimeLibrary.GetExport(module, "lks_invoke"));
        }
        catch
        {
            _library.Dispose();
            throw;
        }
    }

    /// <summary>Invoke ABI 1 JSON operations. All result and callback buffers are scoped to this call.</summary>
    public JsonElement Invoke(object input, Func<HttpRequest, HttpResponse>? transport = null,
                              Func<string, string?>? keyProvider = null)
    {
        ArgumentNullException.ThrowIfNull(input);
        var lease = false;
        GCHandle pinned = default;
        nint output = 0;
        var buffers = new List<nint>();
        ExceptionDispatchInfo? callbackError = null;
        Callback callback = (nint user, nint request, nuint size, out nint response, out nuint responseSize) =>
        {
            response = 0;
            responseSize = 0;
            try
            {
                using var document = JsonDocument.Parse(ReadBytes(request, size));
                var data = document.RootElement;
                object payload;
                switch (data.GetProperty("kind").GetString())
                {
                    case "transport":
                        if (transport is null) throw new InvalidOperationException("transport callback is required");
                        var headers = data.GetProperty("headers").EnumerateArray().Select(pair =>
                            new KeyValuePair<string, string>(pair[0].GetString()!, pair[1].GetString()!)).ToArray();
                        var message = new HttpRequest(data.GetProperty("url").GetString()!,
                            data.GetProperty("method").GetString()!, headers,
                            Wire.DecodeBase64Url(data.GetProperty("body").GetString()!),
                            long.Parse(data.GetProperty("timeout_ms").GetString()!, CultureInfo.InvariantCulture));
                        var result = transport(message) ?? throw new InvalidOperationException("transport returned null");
                        payload = new
                        {
                            status_code = result.StatusCode.ToString(CultureInfo.InvariantCulture),
                            body = Wire.Base64Url(result.Body.Span)
                        };
                        break;
                    case "key":
                        if (keyProvider is null) throw new InvalidOperationException("key provider callback is required");
                        payload = new { secret = keyProvider(data.GetProperty("key").GetString()!) ?? "" };
                        break;
                    default:
                        throw new InvalidOperationException("unknown native callback kind");
                }
                var bytes = JsonSerializer.SerializeToUtf8Bytes(payload);
                var buffer = Marshal.AllocHGlobal(bytes.Length);
                try { buffers.Add(buffer); }
                catch { Marshal.FreeHGlobal(buffer); throw; }
                Marshal.Copy(bytes, 0, buffer, bytes.Length);
                response = buffer;
                responseSize = (nuint)bytes.Length;
                return 0;
            }
            catch (Exception error)
            {
                // Reverse P/Invoke must never unwind a managed exception through the C++ stack.
                callbackError ??= ExceptionDispatchInfo.Capture(error);
                return 1;
            }
        };
        try
        {
            _library.DangerousAddRef(ref lease);
            var bytes = JsonSerializer.SerializeToUtf8Bytes(input);
            pinned = GCHandle.Alloc(bytes, GCHandleType.Pinned);
            output = _invoke(pinned.AddrOfPinnedObject(), (nuint)bytes.Length, callback, 0, out var length);
            callbackError?.Throw();
            if (output == 0) throw new OutOfMemoryException("native C ABI could not allocate or encode its result");
            using var document = JsonDocument.Parse(ReadBytes(output, length));
            var result = document.RootElement;
            if (!result.GetProperty("ok").GetBoolean())
                throw new LiveKitException(result.GetProperty("message").GetString()!,
                    result.GetProperty("code").GetInt32(), result.GetProperty("http_status").GetInt32(),
                    result.GetProperty("twirp_code").GetString()!);
            return result.GetProperty("value").Clone();
        }
        finally
        {
            if (output != 0) _free(output);
            foreach (var buffer in buffers) Marshal.FreeHGlobal(buffer);
            if (pinned.IsAllocated) pinned.Free();
            GC.KeepAlive(callback);
            if (lease) _library.DangerousRelease();
        }
    }

    private static byte[] ReadBytes(nint pointer, nuint length)
    {
        var bytes = new byte[checked((int)length)];
        Marshal.Copy(pointer, bytes, 0, bytes.Length);
        return bytes;
    }

    public void Dispose() => _library.Dispose();

    [DllImport("kernel32", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern nint AddDllDirectory(string directory);
    [DllImport("kernel32", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool RemoveDllDirectory(nint cookie);
}
