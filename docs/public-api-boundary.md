# Public API and Protobuf Compatibility

The primary Room, Egress, Ingress, SIP, Agent Dispatch, and Connector APIs accept and return
distinct types from `livekit::server::model`. These public model headers use only the C++ standard
library. Their payload uses the canonical protobuf JSON mapping, which keeps the SDK boundary
independent of generated C++ classes while retaining a lossless representation of known protocol
fields.

## SDK-owned models

Construct requests with `FromJson()` and inspect responses with `Json()`:

```cpp
const auto request = livekit::server::model::CreateRoomRequest::FromJson(
    R"({"name":"support","emptyTimeout":300})");
const auto room = api.Room().CreateRoom(request);
std::cout << room.Json() << '\n';
```

No-request convenience calls that already returned protobuf values keep their original signatures
for source compatibility. Use `RoomServiceClient::ListRoomsModel()` and
`AgentDispatchClient::GetDispatchModel()` for their SDK-model equivalents.

JSON is converted to the bundled LiveKit Protocol revision immediately before a request is sent.
Malformed JSON, unknown fields, and unknown symbolic enum names throw
`livekit::server::Error` with `ErrorCode::protocol`. Numeric enum values remain representable for
forward compatibility. Unknown response fields are not interpreted by this SDK revision; update
the SDK's protocol revision when those fields are required.

`ErrorCode` has a fixed `std::uint8_t` representation and stable numeric values. Code handling
errors should include a `default` branch or handle `ErrorCode::unknown`, because later SDK versions
may introduce additional categories. The original Twirp code remains available through
`Error::twirp_code()`.

## Optional protobuf adapter

The original protobuf overloads remain available as a source-migration path. To obtain their
generated headers, link the explicit adapter target:

```cmake
find_package(LiveKitServer CONFIG REQUIRED)
target_link_libraries(my_backend PRIVATE LiveKitServer::protobuf_adapter)
```

Then include the protocol header used by the call site:

```cpp
#include <livekit_room.pb.h>

livekit::CreateRoomRequest request;
request.set_name("support");
const auto room = api.Room().CreateRoom(request);
```

The adapter is enabled by default for compatibility. Configure the SDK with
`-DLIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER=OFF` to omit its target and all generated `*.pb.h` files
from the installed package. `LiveKitServer_PROTOBUF_ADAPTER_AVAILABLE` reports whether an installed
package contains it.

The SDK links protobuf as a normal binary dependency. It never merges protobuf static objects into
the SDK archive, avoiding duplicate-symbol and ABI conflicts in applications that already use
protobuf.
