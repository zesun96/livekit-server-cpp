# Zig server binding

Zig module `livekit_server` for this repository's LiveKit C++ server SDK through public C ABI 1.
It exposes all six services, 52 typed service methods, 86 JSON models, Video/SIP/Agent token grants,
verified webhook snapshots and custom binary HTTP callbacks. No Zig-specific C++ bridge, protobuf
generation, external Zig dependencies or Zig HTTP/crypto backend is required. Production protocol,
HTTP, signing and verification stay in the native SDK. RTC/media clients belong to the separate
client SDK; this module is separate from official LiveKit language packages.

**Toolchain:** implemented and validated with Zig **`0.18.0-dev.35+5e754304d`** on Windows x64,
using the existing shared libcurl/OpenSSL C++ SDK. `build.zig.zon` records that minimum version.
Zig's development APIs can change: other development snapshots, stable Zig releases, Linux/macOS,
other architectures, mobile/browser targets and real LiveKit server integration are unvalidated.
No claim of compatibility with Zig 0.15/0.16/0.17 is made.

## Build and import

Build a matching shared SDK separately; reuse an existing compatible shared build when available:

```powershell
# Run from livekit-server-cpp, using the existing workspace protocol/protobuf.
cmake -S . -B out/build/zig-native -G "Visual Studio 17 2022" -A x64 `
  -DBUILD_SHARED_LIBS=ON `
  -DLIVEKIT_SERVER_BUILD_TESTS=OFF -DLIVEKIT_SERVER_BUILD_INTEGRATION_TESTS=OFF `
  -DLIVEKIT_SERVER_BUILD_EXAMPLES=OFF -DLIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER=OFF `
  -DLIVEKIT_PROTOCOL_ROOT="../livekit-client-cpp/protocol" `
  -DCMAKE_PREFIX_PATH="../livekit-client-cpp/out/build/vs2022-x64-release/vcpkg_installed/x64-windows"
cmake --build out/build/zig-native --config Release --parallel
$env:LIVEKIT_SERVER_ZIG_LIBRARY = "$PWD/out/build/zig-native/Release/livekitserver.dll"
$env:Path = "$PWD/../livekit-client-cpp/out/build/vs2022-x64-release/vcpkg_installed/x64-windows/bin;$env:Path"
$env:ZIG_GLOBAL_CACHE_DIR = "$PWD/out/zig-global-cache"
zig build --build-file pkg/zig/build.zig --cache-dir out/zig-local-cache --prefix out/dist/zig
```

This compiles the room-listing example without contacting a server. Windows uses WinAPI module
loading because this Zig snapshot's `std.DynLib` does not expose a Windows backend. Unix targets
use `std.DynLib` with libc; those builds still require platform verification. Deploy native
dependencies on the loader path before starting the process. Portable Windows builds also need
curl/OpenSSL DLLs. The module always frees results through `lks_free` in the allocating SDK, so
Zig and the C++ library can use their respective allocators. See [backend configuration](../../docs/platform-backends.md).

For a local application, add a path dependency in its `build.zig.zon`:

```zig
.dependencies = .{
    .livekit_server_cpp = .{ .path = "/absolute/path/to/livekit-server-cpp/pkg/zig" },
},
```

Then in `build.zig`, import the dependency module into your executable/test module:

```zig
const sdk = b.dependency("livekit_server_cpp", .{ .target = target, .optimize = optimize });
app_module.addImport("livekit_server", sdk.module("livekit_server"));
```

The package exports source only and does not build, bundle or download native libraries. Its
`.paths` list includes the build metadata, source, examples, tests, README and license; the
generator is a repository maintenance tool. This task does not publish a package or registry entry.

## Ownership and results

```zig
const std = @import("std");
const sdk = @import("livekit_server");

fn listRooms(allocator: std.mem.Allocator, library_path: []const u8) !void {
    // Trust the selected module, initializers/destructors and all loader dependencies.
    const native = try sdk.NativeLibrary.open(allocator, library_path);
    defer native.release();
    var api = try sdk.LiveKitApi.init(allocator, native, .{});
    defer api.deinit();
    var request = try sdk.models.ListRoomsRequest.empty(allocator);
    defer request.deinit();
    var result = try api.room().listRooms(allocator, request);
    defer result.deinit();
    switch (result) {
        .value => |rooms| std.debug.print("{s}\n", .{rooms.json}),
        .failure => |failure| std.debug.print("SDK {d}, HTTP {d}, Twirp {s}: {s}\n",
            .{ failure.code, failure.http_status, failure.twirp_code, failure.message }),
    }
}
```

`NativeLibrary.open` checks `lks_abi_version() == 1` and all three exports. A version number cannot
prove a foreign library's signatures or lifetime rules: only load a trusted implementation of
the complete [C ABI contract](../../docs/c-abi.md), including synchronous same-thread callbacks,
valid length-delimited buffers, concurrent independent calls and no escaping C++ exceptions.
Loader errors, missing exports and incompatible versions are ordinary Zig errors.

The returned module pointer owns one reference. `retain()` adds a reference, `release()` removes
one, and the final release unloads the module and frees its handle. APIs, token builders and
receivers retain their module independently. Calls take a temporary module lease through result
cleanup. The caller still owns a live reference when starting a call; never use a released pointer.
The library path is explicit. `default_filename` supplies the platform filename, and the example
reads `LIVEKIT_SERVER_ZIG_LIBRARY` from `std.process.Init`'s environment map.

API/options strings, token attributes/grants and webhook key maps are copied into their owner's
arena. Models own copied JSON bytes; parsed inspection values are separately owned. Successful
RPC models, token `OwnedString.bytes`, webhook events, SDK failures and optional dispatch values
are owned by their returned `Result(T)`; deinitialize the **result** exactly once. Do not also
deinitialize its payload. Models created for requests must be deinitialized separately.

Owning Zig structs must not be bit-copied and independently deinitialized. Borrow a pointer for
sharing; use a model's `clone(allocator)` when an independent copy is needed. Allocators must
outlive all objects/results allocated with them. No object may be deinitialized while another
thread uses it. Native handles use atomic reference counts; concurrent calls need thread-safe
allocators and host contexts. Token setters need exclusive access; read-only signing can run
concurrently after configuration. Arenas retain replaced option/grant values until owner deinit.

The outer `!` error union carries local allocation/argument/JSON/loader failures and the original
host callback error value. Native SDK errors use `Result(T).failure` with owned `code`, `message`,
`http_status` and `twirp_code`; numeric constants match ABI 1 and future codes are preserved.
There is no shared or thread-local last-error slot. Generic Zig struct formatting can expose
credentials; do not dump API/options, token builders, receivers or application contexts to logs.

## Services, models and transports

Services are `api.room()`, `egress()`, `ingress()`, `sip()`, `agentDispatch()` and `connector()`.
Methods use lower-camel-case names matching C++ model overloads. For example:
`api.sip().createInboundTrunk(allocator, request)`. Pass a typed request to every service method.
`agentDispatch().getDispatch(allocator, id, room)` returns `!Result(?models.AgentDispatch)`;
a null successful value means no dispatch was returned.

Models provide `empty(allocator)`, `initJson(allocator, raw_json)`, `fromValue(allocator, value)`,
`toValue(allocator)`, `clone(allocator)`, `json`, and `deinit()`. `fromValue` accepts JSON-serializable
structs or `std.json.Value` and requires an object. `initJson` preserves raw text for native schema
validation on invocation. `toValue` returns a detached `std.json.Parsed(Value)` that the caller
deinitializes. Keep `.json` read-only. Use canonical protobuf JSON field names, base64 for bytes
and decimal strings for 64-bit integers. There are no generated per-field protobuf accessors.

`ApiOptions` accepts `url`, `api_key`, `api_secret`, `access_token`, positive `timeout_ms` (default
30000), and an optional `Transport`. Empty URL/credentials use the native `LIVEKIT_*` environment
fallback. Omit the transport for native HTTP, or supply:

```zig
fn send(context: ?*anyopaque, request: sdk.HttpRequest,
        temporary: std.mem.Allocator) anyerror!sdk.HttpResponse {
    _ = context;
    _ = request;
    // Adapt your HTTP client here. This offline stub returns an empty protobuf message.
    return .{ .status_code = 200, .body = try temporary.dupe(u8, "") };
}
// ApiOptions{ .transport = .{ .context = application_context, .send = send }, ... }
```

The transport receives URL/method, ordered header pairs, binary protobuf `body`, and timeout.
It must honor timeouts and TLS verification. Success bodies are protobuf bytes, failure bodies
are normally Twirp JSON. Borrowed request fields and the supplied arena allocator are valid only
through the enclosing invoke. Allocate response bytes with that allocator, or return static/
context-backed data valid through encoding; never return a slice into callback stack storage.
Do not free arena response bytes after returning. The bridge serializes/copies them before the
arena is released. Application HTTP response objects should be cleaned up inside the adapter after
copying their bytes to the provided allocator.

Transport and key-provider errors are saved in the per-call context, reported to C as nonzero,
then returned unchanged after native result cleanup. Zig panics do not unwind as recoverable error
values: a panic/trap/abort terminates execution and is not caught at this boundary. Callbacks run
synchronously on the invoking thread. Nested calls on the same module and independent concurrent
calls are supported; there are no background callbacks, built-in async calls or cancellation.

## Tokens and webhooks

Create `AccessToken.init(allocator, native, key, secret)` and configure it with `setIdentity`,
`setName`, `setMetadata`, `setAttributes([]const AccessToken.Attribute)`, `setValidFor(seconds)`,
`setVideoGrant`, `setSipGrant` and `setAgentGrant`. Setters return `!void`. Grant field names are
ABI snake_case. Optional booleans use `?bool`: null omits the field, false is preserved explicitly.
String/source slices are snapshotted by the setter, so later caller changes do not affect signing.
`toJwtAt(allocator, unix_seconds)` supplies a clock; `toJwt(allocator, io)` uses the real clock
through `std.Io`. Native code validates time, lifetime and expiry overflow. Success is an owned
JWT string in `Result(OwnedString).value.bytes`. Signing remains in C++.

`WebhookReceiver.init` accepts a key/secret pair. `initSecrets` accepts `[]const Secret`;
`initKeyProvider` accepts a `KeyProvider` with a context and
`fn(context, issuer, temporary_allocator) anyerror!?[]const u8`. Return null for unknown issuers;
temporary secret strings can use that allocator. Provider contexts are host-owned.

`receive(allocator, exact_raw_http_body, authorization)` verifies before constructing an event.
Pass exact request bytes, not reserialized JSON. The successful `WebhookEvent.snapshot` exposes
`event`, `id`, integer `created_at`, complete verified `raw_body`, and optional typed `room`,
`participant`, `track`, `egress` and `ingress` records. Unknown fields remain in `raw_body`.
Snapshot slices are borrowed from the owning result and expire at result deinit.

`setCallback(.{ .context = ..., .deliver = handler })` installs a
`fn(context, *const WebhookSnapshot) anyerror!void`; null clears it. `dispatch(&snapshot)` calls
the current handler; `receiveAndDispatch` verifies first and cleans the event up on handler error.
Callback replacement uses a short atomic lock, and invocation happens outside it, supporting
replacement/reentrant dispatch. Contexts remain host-owned and must outlive all active dispatches,
including dispatches that captured a callback before replacement. Receiver copies are not a
supported sharing mechanism; share a stable pointer and keep it alive.

## Offline validation and source packaging

From the repository root, with the cache path configured as above:

```powershell
zig build --build-file pkg/zig/build.zig --cache-dir out/zig-local-cache test --summary all
zig build --build-file pkg/zig/build.zig --cache-dir out/zig-local-cache test-native `
  "-Dnative-library=$env:LIVEKIT_SERVER_ZIG_LIBRARY" --summary all
zig fmt --check pkg/zig/build.zig pkg/zig/build.zig.zon pkg/zig/src pkg/zig/test pkg/zig/examples
python pkg/zig/scripts/generate_api.py
# Compile/run a separate b.dependency consumer (also included in source archives):
zig build --build-file pkg/zig/test/consumer/build.zig --cache-dir out/zig-consumer-cache test `
  "-Dnative-library=$env:LIVEKIT_SERVER_ZIG_LIBRARY" --summary all
```

Five default tests cover all 86 models, detached snapshots, unknown failure codes, allocation/free
counts on success/error/null/host-failure paths and exhaustive allocation failure cleanup across
models, callbacks, API calls, token setters and webhook conversion. Eight opt-in native cases cover
all 52 routes, binary/Unicode/NUL data, independent Zig HMAC/SHA-256 checks, webhook tampering and
lifetimes, host error values, callback reentrancy, module retention, and eight concurrent threads
performing 80 nested calls. No sockets, LiveKit server or physical devices are used. Omitting the
native build option from `test-native` fails explicitly; ordinary `test` needs no shared SDK.

Zig's `.paths` manifest supports local source dependencies and source archives. Validation also
uses a separate application importing an extracted source archive through `b.dependency` and runs
the packaged tests. Build caches, binaries, native dependencies and logs must stay out of archives.
The [room-listing example](examples/list_rooms.zig) has been compiled only; running it explicitly
contacts the server configured by environment credentials. Real-server binding integration has
not been exercised.
