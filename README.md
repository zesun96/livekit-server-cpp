# LiveKit Server C++ SDK

A standalone C++20 SDK for LiveKit server APIs and access-token creation. It is intentionally
separate from `livekit-client-cpp`: backend control-plane code does not pull in libwebrtc, media
capture, or playback dependencies.

## Platform status

| Platform | Status |
| --- | --- |
| Windows x64 | Native SDK and Java, Ruby, Python, .NET, PHP, Rust, Zig bindings validated locally |
| GNU/Linux | libcurl + OpenSSL backends implemented; platform/bindings not validated |
| macOS | libcurl + OpenSSL backends implemented; platform/bindings not validated |
| iOS / Android | No supported target declared |

Linux/macOS select libcurl (>= 7.85) and OpenSSL (>= 3.0) by default for HTTP(S), SHA-256,
HS256 signing and webhook verification. These implementations still require target-platform
validation. A portable library reduces OS-specific code; it does not replace platform testing.
See [portable backend build and validation](docs/platform-backends.md).

## Implemented services

- Room and participant administration
- Egress and Ingress
- SIP trunks, dispatch rules, and participants
- Agent dispatch
- WhatsApp and Twilio connectors
- Verified LiveKit webhook event callbacks
- LiveKit access tokens signed with HS256
- Twirp-over-HTTP protobuf transport, with connection reuse through WinHTTP or libcurl

The primary service API uses SDK-owned models and does not require generated protobuf headers:

```cpp
#include <livekit/server/livekit_api.h>

livekit::server::ApiOptions options;
options.url = "https://project.livekit.cloud";
options.api_key = "api-key";
options.api_secret = "api-secret";

livekit::server::LiveKitApi api(std::move(options));

const auto request = livekit::server::model::CreateRoomRequest::FromJson(
    R"({"name":"support"})");
const auto room = api.Room().CreateRoom(request);
std::cout << room.Json() << '\n';
```

SDK models contain canonical protobuf JSON and have distinct C++ types for every request and
response. They keep the API boundary stable while allowing new protocol fields to be represented
without adding generated dependencies to public headers. See
[`docs/public-api-boundary.md`](docs/public-api-boundary.md) for validation and compatibility
behavior.

Existing protobuf call sites remain supported through an optional compatibility target. It is
enabled by default and installs the generated protocol headers:

```cmake
target_link_libraries(my_backend PRIVATE LiveKitServer::protobuf_adapter)
```

Set `LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER=OFF` when building or packaging the SDK to omit the
generated headers and adapter target. Consumers using only SDK models continue to link
`LiveKitServer::livekitserver`.

`LIVEKIT_URL`, `LIVEKIT_API_KEY`, and `LIVEKIT_API_SECRET` are used when the corresponding options
are omitted. A pre-signed token can be supplied with `ApiOptions::access_token` or `LIVEKIT_TOKEN`.
Explicit credentials always take precedence over environment credentials.

Never embed an API secret in a desktop, mobile, browser, or other untrusted client. Such clients
should receive short-lived tokens from an authenticated backend. Applications that deliberately
call a limited server API with a pre-signed token can link this SDK separately from the RTC client.

## Token creation

```cpp
livekit::server::VideoGrant grant;
grant.room_join = true;
grant.room = "support";
grant.can_publish = true;
grant.can_subscribe = true;

const auto token = livekit::server::AccessToken("api-key", "api-secret")
                       .SetIdentity("agent-1")
                       .SetName("Support agent")
                       .SetValidFor(std::chrono::hours(1))
                       .SetVideoGrant(std::move(grant))
                       .ToJwt();
```

## Webhook event callbacks

LiveKit server events arrive through signed HTTP webhooks. `WebhookReceiver` verifies the HS256
JWT, token lifetime, API key, and SHA-256 checksum of the exact request body before parsing or
dispatching the event:

```cpp
#include <livekit/server/webhook_receiver.h>

livekit::server::WebhookCallbacks callbacks;
callbacks.on_participant_joined = [](const livekit::server::WebhookEvent& event) {
  if (event.participant) {
    std::cout << event.participant->identity << " joined\n";
  }
};
callbacks.on_room_finished = [](const livekit::server::WebhookEvent& event) {
  if (event.room) {
    std::cout << event.room->name << " finished\n";
  }
};

livekit::server::WebhookReceiver receiver(
    "api-key", "api-secret", std::move(callbacks));

// Inside the application's HTTP POST handler. Pass the raw, unmodified body;
// reserializing JSON before verification changes its checksum.
receiver.ReceiveAndDispatch(raw_request_body, authorization_header);
```

Callbacks are available for room, participant, track, Egress, and Ingress events. `on_event` runs
for every verified event, including event types added by a newer server. Event-specific callbacks
run after `on_event`. Dispatch is synchronous on the calling HTTP-handler thread; applications can
hand work to their own executor when callbacks must not block the response. `SetCallbacks()` may be
called concurrently with event processing. The public webhook types use only the C++ standard
library and do not expose generated protobuf headers. `WebhookEvent::raw_body` preserves the full
verified JSON payload for protocol fields not present in the convenience structures.

Multiple signing keys can be supplied as a map. `WebhookKeyProvider` supports dynamic secret
lookup without storing the whole key set in the receiver. Authentication and malformed-payload
failures throw `livekit::server::Error`; return a non-success HTTP status and never process the
unverified body.

## Build

The SDK requires protobuf. By default CMake downloads the checksum-pinned LiveKit Protocol source
archive. For local or offline work, point `LIVEKIT_PROTOCOL_ROOT` at an existing checkout:

```powershell
cmake -S . -B out/build/vs2022-x64-release `
  -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DLIVEKIT_PROTOCOL_ROOT=../livekit-client-cpp/protocol

cmake --build out/build/vs2022-x64-release --config Release --parallel
ctest --test-dir out/build/vs2022-x64-release -C Release -L unit --output-on-failure
```

When tests are enabled, CMake uses the same GoogleTest 1.15.2 setup as `livekit-client-cpp` and
registers individual cases through `gtest_discover_tests`. Set `USE_SYSTEM_GTEST=ON` to use an
installed GoogleTest package instead of the checksum-pinned source archive.

Consumers link the namespaced target:

```cmake
find_package(LiveKitServer CONFIG REQUIRED)
target_link_libraries(my_backend PRIVATE LiveKitServer::livekitserver)
```

On MSVC, the application, this SDK, and protobuf must use the same runtime library. The default
vcpkg `x64-windows` triplet uses `/MD`; select a consistently static triplet only when every linked
component is built with the matching `/MT` setting. Keep the LiveKit Protocol revision aligned
when linking this library together with another SDK that also supplies generated LiveKit messages.

## Transport and errors

Windows uses WinHTTP and Windows CNG, so no extra HTTP, TLS, or cryptography dependency is needed.
`ApiOptions::transport` accepts an application-owned implementation for tests, custom proxy or
certificate policy, and alternative HTTP libraries.

Server and transport failures throw `livekit::server::Error`. It includes an error category, HTTP
status, and Twirp error code where available. Every logical call includes a stable
`X-Livekit-Request-Id` header.

## Language bindings

| Language | Available API | Guide |
| --- | --- | --- |
| Java 17 | All six server services, 86 JSON models, Video/SIP/Agent token grants, verified webhook snapshots and callbacks, and custom HTTP transport | [Java binding](pkg/java/README.md) |
| Ruby 3.1+ | All six server services, 86 Hash/JSON models, Video/SIP/Agent token grants, verified webhook snapshots and callbacks, and custom HTTP transport | [Ruby binding](pkg/ruby/README.md) |
| Python 3.10+ | All six server services, 86 mapping/JSON models, token grants, verified webhook snapshots and callbacks, and custom HTTP transport through ctypes | [Python binding](pkg/python/README.md) |
| .NET 8+ | All six server services, 86 JSON models, token grants, verified webhook snapshots, and custom binary HTTP transport through the public C ABI | [.NET binding](pkg/dotnet/README.md) |
| PHP 8.2+ (64-bit) | All six server services, 86 JSON models, token grants, verified webhook snapshots, and custom binary HTTP transport through FFI | [PHP binding](pkg/php/README.md) |
| Rust 1.87+ | All six server services, 86 serde/JSON models, token grants, verified webhook snapshots, and custom binary HTTP callbacks through the public C ABI | [Rust binding](pkg/rust/README.md) |
| Zig 0.18.0-dev.35 | All six server services, 86 JSON model types, token grants, verified webhook snapshots, and custom binary HTTP callbacks through the public C ABI | [Zig binding and exact toolchain](pkg/zig/README.md) |

All seven bindings expose the 52 model-based service methods for Room, Egress, Ingress, SIP,
AgentDispatch, and Connector. They use the C++ SDK for request validation, authentication,
transport, and error handling. Requests and responses follow canonical protobuf JSON; language
consumers do not need generated protobuf classes. Calls are synchronous, with application-owned
executors or threads available for asynchronous use.

The Java package uses a JNI bridge and has no third-party Java dependencies. Build the native
bridge with CMake, then compile/package Java with the JDK or Gradle. Set
`livekit.server.java.library` to the built bridge's absolute path. See the
[Java examples](pkg/java/examples/README.md) for a room-listing request.

The Ruby gem loads its C ABI bridge through Fiddle and imports as `require 'livekit/server'`.
Build the bridge with CMake and the gem with `gem build`, then set `LIVEKIT_SERVER_RUBY_LIBRARY`
to the bridge path. Fiddle, JSON, and Base64 are the runtime gem dependencies. The gem does not
automatically build or bundle the native SDK. The [Ruby example](pkg/ruby/examples/list_rooms.rb)
lists rooms using environment credentials.

The Python package imports as `livekit_server` and loads the shared SDK directly through ctypes.
Build the SDK with `BUILD_SHARED_LIBS=ON` and native tests/examples OFF, install `pkg/python`, then
set `LIVEKIT_SERVER_PYTHON_LIBRARY` to the shared library path. It needs no third-party Python
runtime packages or Python-specific native bridge. See the [Python guide](pkg/python/README.md).

The .NET package `LiveKit.Server.Cpp` uses namespace `LiveKit.Server` and loads C ABI exports
through managed interop. Build the shared SDK, set `LIVEKIT_SERVER_DOTNET_LIBRARY`, and reference
the managed project or its local NuGet package. No .NET-specific C++ bridge or third-party managed
dependency is required. See the [.NET guide](pkg/dotnet/README.md).

The PHP Composer package `livekit/server-sdk-cpp` uses namespace `LiveKit\Server` and loads the
shared SDK directly through `ext-ffi`. Set `LIVEKIT_SERVER_PHP_LIBRARY`, enable FFI in the PHP
process, and deploy native dependencies on the OS loader path. It requires no PHP-specific C++
bridge or third-party PHP libraries. See the [PHP guide](pkg/php/README.md).

The Rust crate `livekit-server-cpp` imports as `livekit_server_cpp` and loads the shared SDK with
`libloading`. Loading a trusted ABI 1 module requires an explicit unsafe call; subsequent SDK
operations use safe Rust APIs. Arc ownership retains the module through native cleanup, and
callbacks preserve application errors and resume panics after returning from C++. Set
`LIVEKIT_SERVER_RUST_LIBRARY`; see the [Rust guide](pkg/rust/README.md).

The Zig module `livekit_server` loads the shared SDK directly and uses explicit allocators,
owned results, `deinit` and reference-counted module `release`. Native errors retain their details,
and host callback errors return after native cleanup. The package has no external Zig dependencies.
See the [Zig guide](pkg/zig/README.md) for its exact development-toolchain version and ownership rules.

The Java/Ruby bridges are built separately from the main SDK build and must match the language runtime's
architecture. Keep native compiler, runtime, and protobuf dependencies consistent; the binding
guides include Windows static-runtime build commands. No explicit API-object cleanup is needed:
each synchronous call owns its native state. See the [binding support matrix](pkg/README.md) for
API coverage, packaging, examples, validation, and platform limits.

## Tests

Unit tests use an in-memory transport and never access the network. Server integration tests are
opt-in:

```powershell
cmake -S . -B out/build/integration -DLIVEKIT_SERVER_BUILD_INTEGRATION_TESTS=ON
ctest --test-dir out/build/integration -C Release -L integration --output-on-failure
```

Integration tests read credentials from the `LIVEKIT_*` environment variables and skip when they
are unavailable. The webhook integration test listens only on `127.0.0.1:17890`; configure the
local LiveKit Server webhook URL to that address before running it. It creates and deletes a unique
room and requires a `room_started` event to pass signature verification and reach the callback.

The Java, Ruby, Python, .NET, PHP, Rust and Zig bindings have separate offline smoke tests covering all service routes, binary
and Unicode payloads, token signing, webhook verification and callback exceptions. Java, Ruby,
Python, .NET, Rust and Zig also exercise concurrent calls; PHP exercises nested synchronous calls.
Ruby also exercises GC and compaction; Python checks callback GC and result cleanup. These use in-memory transports;
running them does not require a server or open network connections. Build and run commands are in
the [binding guides](pkg/README.md#offline-validation). Real-server binding integration has not
been validated; the room-listing examples contact a server only when explicitly run.

## Documentation

| Topic | Document |
| --- | --- |
| SDK-owned models and protobuf compatibility | [Public API boundary](docs/public-api-boundary.md) |
| Language API, example, and platform coverage | [Language bindings](pkg/README.md) |
| Java build, packaging, tokens, and webhooks | [Java guide](pkg/java/README.md) |
| Ruby build, gem packaging, tokens, and webhooks | [Ruby guide](pkg/ruby/README.md) |
| Python wheel, ctypes, tokens, and webhooks | [Python guide](pkg/python/README.md) |
| .NET NuGet, C ABI interop, tokens, and webhooks | [.NET guide](pkg/dotnet/README.md) |
| PHP Composer, FFI, tokens, and webhooks | [PHP guide](pkg/php/README.md) |
| Rust crate, native module ownership, tokens, and webhooks | [Rust guide](pkg/rust/README.md) |
| Zig module, explicit allocation, tokens, and webhooks | [Zig guide](pkg/zig/README.md) |
| C++ room administration example | [Room service](examples/room_service.cpp) |
| C++ webhook handler example | [Webhook handler](examples/webhook_handler.cpp) |

## Shared C ABI

All language bindings use the [public C ABI](docs/c-abi.md) in
`include/livekit/server/c_api.h`: versioned entry points, stable error values, library-owned
result buffers, and synchronous callbacks with explicit context and byte lengths. Ruby retains
its previous symbols as compatibility wrappers; JNI translates Java values and exceptions; Python
uses ctypes directly; .NET uses managed interop delegates and module leases; PHP uses FFI;
Rust uses libloading and C function pointers; Zig uses C function pointers and explicit allocation.
Each retains per-call
callback buffers and frees native results through the allocating library.
