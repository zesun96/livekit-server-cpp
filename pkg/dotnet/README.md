# .NET server binding

.NET 8+ managed binding for the LiveKit server C++ SDK through public C ABI 1. Namespace:
`LiveKit.Server`; NuGet package: `LiveKit.Server.Cpp`. It exposes all 52 model-based service methods,
86 distinct JSON models, Video/SIP/Agent token grants, verified webhook snapshots and custom binary
HTTP transports. It uses `System.Text.Json` and the .NET runtime; no third-party NuGet packages,
generated protobuf classes or .NET-specific native bridge are required.

The package contains managed assemblies only. Deploy the C++ shared SDK and its native dependencies
separately, for the same architecture as the .NET process. Windows x64 / .NET 8 is validated locally
with libcurl/OpenSSL. Linux/macOS, other architectures and .NET versions remain unvalidated. .NET
Framework, NativeAOT, trimming and browser/WASM targets are not declared supported.

## Build and package

Run from the `livekit-server-cpp` repository root. Reuse the workspace protocol and protobuf:

```powershell
cmake -S . -B out/build/dotnet-native -G "Visual Studio 17 2022" -A x64 `
  -DBUILD_SHARED_LIBS=ON `
  -DLIVEKIT_SERVER_BUILD_TESTS=OFF -DLIVEKIT_SERVER_BUILD_INTEGRATION_TESTS=OFF `
  -DLIVEKIT_SERVER_BUILD_EXAMPLES=OFF -DLIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER=OFF `
  -DLIVEKIT_PROTOCOL_ROOT="../livekit-client-cpp/protocol" `
  -DCMAKE_PREFIX_PATH="../livekit-client-cpp/out/build/vs2022-x64-release/vcpkg_installed/x64-windows"
cmake --build out/build/dotnet-native --config Release --parallel
$env:LIVEKIT_SERVER_DOTNET_LIBRARY = "$PWD/out/build/dotnet-native/Release/livekitserver.dll"
$env:LIVEKIT_SERVER_DOTNET_DLL_DIRECTORIES = "$PWD/../livekit-client-cpp/out/build/vs2022-x64-release/vcpkg_installed/x64-windows/bin"
dotnet build pkg/dotnet/src/LiveKit.Server.Cpp -c Release
dotnet pack pkg/dotnet/src/LiveKit.Server.Cpp -c Release -o out/dist/dotnet
```

The package's `NuGet.Config` clears remote feeds because development projects need only installed
.NET framework references. No downloads are required with the .NET 8 targeting pack installed.
Normalize duplicate PATH/Path process keys as described in AGENTS.md if MSBuild fails before compiling.

Add a project reference to `pkg/dotnet/src/LiveKit.Server.Cpp/LiveKit.Server.Cpp.csproj`, or consume
the local package with `dotnet add <your-project> package LiveKit.Server.Cpp --version 0.1.0
--source <absolute-path-to-out/dist/dotnet>`. Packing does not build or include native libraries.
Keep protobuf and the SDK's compiler/runtime configurations consistent. Linux/macOS select
libcurl/OpenSSL by default; Windows defaults to WinHTTP/CNG and can select the portable backends.
See [platform backends](../../docs/platform-backends.md). The native SDK in this revision also
accepts Unicode-escaped JWT string claims, including checksums containing `\u002B` from
`System.Text.Json`; rebuild older shared SDK binaries to include that compatibility fix.

## Loading and lifetime

Set `LIVEKIT_SERVER_DOTNET_LIBRARY` before the first SDK operation to the full path of
`livekitserver.dll`, `liblivekitserver.so` or `liblivekitserver.dylib`. Without it, the runtime
searches for that platform filename. The default module is loaded once, lazily, and retained for
the process lifetime. Configuring clients, models, tokens or receivers does not load native code.
The default lazy loader caches initialization failures too; configure it before first use.

Alternatively own an explicit module:

```csharp
using LiveKit.Server;

using var native = new NativeServer("/absolute/path/to/shared/library",
                                  dllDirectories: new[] { "/path/to/dependencies" });
var api = new LiveKitApi(new ApiOptions { Url = "https://example.livekit.cloud" }, native);
```

On Windows, put dependency DLLs beside the SDK DLL or supply semicolon-separated directories via
`LIVEKIT_SERVER_DOTNET_DLL_DIRECTORIES` / `dllDirectories`. Directory entries are added only while
loading and removed afterwards. On Linux/macOS, use the OS's shared-library deployment and loader
paths. Missing files/dependencies report `DllNotFoundException`; missing exports report
`EntryPointNotFoundException`; a version other than ABI 1 reports `NotSupportedException`.

An explicit `NativeServer` owns its module through `SafeHandle`. `Dispose()` rejects future calls;
in-flight invokes retain a lease through native result and callback cleanup and can complete.
Disposal does not cancel them. API/receiver instances borrow a supplied `NativeServer`; they do
not dispose it. Dispose it when all application SDK work is finished. API objects themselves have
no `Dispose()` requirement. Nested/concurrent invokes have independent callback buffers.

## Services and models

```csharp
using LiveKit.Server;
using M = LiveKit.Server.Models;

var api = new LiveKitApi(); // LIVEKIT_URL and API key/secret or LIVEKIT_TOKEN
var room = api.Room.CreateRoom(new M.CreateRoomRequest(new { name = "support", emptyTimeout = 300 }));
Console.WriteLine(room.ToJsonElement().GetProperty("name").GetString());
var rooms = api.Room.ListRooms();
```

Services are `Room`, `Egress`, `Ingress`, `Sip`, `AgentDispatch`, `Connector`. Methods match the
C++ model overloads, e.g. `api.Sip.CreateInboundTrunk(new M.CreateSIPInboundTrunkRequest(...))`.
`AgentDispatch.GetDispatch(id, room)` returns a model or null when the server's filtered response
is empty. [Generated methods](src/LiveKit.Server.Cpp/Services.g.cs) are strongly typed.

Models accept JSON text or an object such as an anonymous object/dictionary. `.Json` preserves
its JSON; `.ToJsonElement()` returns a detached snapshot. Equality/hash include model type and
JSON text. Fields use canonical protobuf JSON names (usually camelCase), bytes use standard
base64, and 64-bit integers should use decimal strings. Request schema validation happens in
C++; unknown fields and malformed payloads raise `LiveKitException`. Models expose JSON rather
than generated per-field protobuf getters. Use camelCase anonymous-object keys as above.

`ApiOptions` exposes init-only `Url`, `ApiKey`, `ApiSecret`, `AccessToken`, positive `TimeoutMs`
(default 30000), and `Transport`. Empty URL/credentials use the core's environment fallback.
Set core `LIVEKIT_*` URL/credential variables before starting the process; applications can also
configure them explicitly through `ApiOptions`. Options and token builders redact credentials
in `ToString()`. Calls are synchronous; use an
application-owned task/worker if required. Cancelling a `Task.Run` wrapper does not stop an
in-flight native request; bound work with `TimeoutMs`.

## Binary HTTP callbacks and exceptions

```csharp
var api = new LiveKitApi(new ApiOptions
{
    Url = "http://offline.invalid", AccessToken = "test-token",
    Transport = request => new HttpResponse(200) // offline empty ListRoomsResponse protobuf
});
var empty = api.Room.ListRooms();
```

`HttpRequest` provides URL, method, an ordered read-only list of header pairs, read-only bytes
and timeout in milliseconds. Return `HttpResponse(statusCode, body)` with binary protobuf on
success or Twirp JSON on failure. Responses snapshot the supplied byte buffer. Honor the timeout
in custom transports. Shared APIs require thread-safe transports.

`LiveKitException` preserves numeric `Code`, enum `ErrorCode`, `HttpStatus`, and `TwirpCode`.
Future codes retain their number and map to `ErrorCode.Unknown`. Callback exceptions are captured
with `ExceptionDispatchInfo` and rethrown after native cleanup, preserving the original instance
and stack. No managed exception unwinds through the reverse P/Invoke/C++ stack. Webhook event
callback exceptions propagate directly.

The adapter uses cdecl delegates and native-width `nuint` lengths. It pins UTF-8 request bytes,
keeps callback delegates alive through return, and retains callback response buffers until invoke
finishes. SDK-owned results always use that module's `lks_free`; managed callback buffers use
`Marshal.FreeHGlobal`. JSON values are cloned before releasing native results. No native code
retains managed objects and no background callback worker is created.

## Tokens

```csharp
var jwt = new AccessToken("test-key", "test-only-secret-at-least-32-characters")
    .SetIdentity("alice").SetName("Alice").SetMetadata("application data")
    .SetValidFor(TimeSpan.FromMinutes(10))
    .SetVideoGrant(new VideoGrant { RoomJoin = true, Room = "support", CanPublish = false })
    .SetSipGrant(new SipGrant { Call = true })
    .SetAgentGrant(new AgentGrant { SimulationAdmin = true })
    .ToJwt();
```

Setters return the builder. String-to-string attributes are copied. Lifetime accepts positive
integer seconds or a whole-second `TimeSpan`. `ToJwt()` uses UTC now; supply `DateTimeOffset`
or Unix seconds for an explicit clock. Clock bounds are checked by C++. Grants use init-only
properties; source lists are copied and exposed read-only. Optional boolean grants use `bool?`:
null omits the claim; explicit false is retained. Do not modify a token builder concurrently.
Signing is always native; the binding does not implement its own JWT algorithm.

## Webhooks

```csharp
var receiver = new WebhookReceiver(
    new Dictionary<string, string> { ["test-key"] = "test-only-secret-at-least-32-characters" },
    callback: value => Console.WriteLine(value.Event));
// In an HTTP handler: receiver.ReceiveAndDispatch(originalRequestBytes, authorizationHeader);
```

Choose key/secret, a secrets dictionary, or `Func<string, string?>` resolving the issuer's current
secret (null for unknown). Dictionaries are copied. The native SDK verifies HS256, lifetime and
exact-body checksum before returning typed records. `WebhookEvent` includes event/id, integer
`CreatedAt`, optional room/participant/track/egress/ingress snapshots, and complete verified
`RawBody`, including future protocol fields. Pass original bytes or original UTF-8 text; do not
parse/re-encode an HTTP body before verification.

`Receive` verifies without dispatching; `Dispatch` calls the host event callback;
`ReceiveAndDispatch` combines them. `SetCallback` atomically replaces or clears the callback.
Invocation runs outside the callback lock and supports re-entry. Share only thread-safe key
providers/callbacks. The receiver does not create an HTTP listener.

## Offline checks and maintenance

```powershell
dotnet run --project pkg/dotnet/test/OfflineSmoke -c Release
# Without an SDK library: explicitly skip native checks.
dotnet run --project pkg/dotnet/test/OfflineSmoke -c Release -- --managed-only
python pkg/dotnet/scripts/generate_api.py
```

The console smoke test uses only .NET framework libraries and fake HTTP transports. It exercises
all 52 routes, 86 models, Unicode/NUL/binary data, independent HMAC, grants, clock/culture handling,
SDK and host exceptions, webhooks and rotating keys, GC, nested/concurrent callbacks, and module
disposal while a request is in flight. No network/server or physical devices are accessed. Run
without `--managed-only` for required native checks; missing native prerequisites then fail.

Validate the packed assembly too (the consumer links the same smoke source against the nupkg):

```powershell
dotnet restore pkg/dotnet/test/PackageConsumer --source "$PWD/out/dist/dotnet" --packages "$PWD/out/nuget-dotnet"
dotnet run --project pkg/dotnet/test/PackageConsumer -c Release --no-restore
```

Build the [read-only room-listing example](examples/ListRooms/Program.cs) with
`dotnet build pkg/dotnet/examples/ListRooms -c Release`; run it only when a real-server request is
intended. Real-server .NET integration has not been validated. Generation reads public C++ headers
and keeps checked-in model/service/grant wrappers reproducible. Shared C ABI mappings remain
managed by `python scripts/generate_c_api.py`.

Interop references: [Native library loading](https://learn.microsoft.com/dotnet/api/system.runtime.interopservices.nativelibrary.load)
and [exception preservation](https://learn.microsoft.com/dotnet/api/system.runtime.exceptionservices.exceptiondispatchinfo).
