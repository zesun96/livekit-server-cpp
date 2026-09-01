# LiveKit Server C++ SDK

A standalone C++20 SDK for LiveKit server APIs and access-token creation. It is intentionally
separate from `livekit-client-cpp`: backend control-plane code does not pull in libwebrtc, media
capture, or playback dependencies.

## Implemented services

- Room and participant administration
- Egress and Ingress
- SIP trunks, dispatch rules, and participants
- Agent dispatch
- WhatsApp and Twilio connectors
- LiveKit access tokens signed with HS256
- Twirp-over-HTTP protobuf transport, with connection reuse through WinHTTP on Windows

The public entry point mirrors the official Go server SDK:

```cpp
#include <livekit/server/livekit_api.h>

livekit::server::ApiOptions options;
options.url = "https://project.livekit.cloud";
options.api_key = "api-key";
options.api_secret = "api-secret";

livekit::server::LiveKitApi api(std::move(options));

livekit::CreateRoomRequest request;
request.set_name("support");
const auto room = api.Room().CreateRoom(request);
```

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
certificate policy, and future non-Windows backends.

Server and transport failures throw `livekit::server::Error`. It includes an error category, HTTP
status, and Twirp error code where available. Every logical call includes a stable
`X-Livekit-Request-Id` header.

## Tests

Unit tests use an in-memory transport and never access the network. Server integration tests are
opt-in:

```powershell
cmake -S . -B out/build/integration -DLIVEKIT_SERVER_BUILD_INTEGRATION_TESTS=ON
ctest --test-dir out/build/integration -C Release -L integration --output-on-failure
```

Integration tests read credentials from the `LIVEKIT_*` environment variables and skip when they
are unavailable.
