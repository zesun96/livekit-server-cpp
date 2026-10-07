#include <livekit/server/livekit_api.h>

#include <livekit_room.pb.h>

void CompileProtobufServiceApi(livekit::server::LiveKitApi& api) {
	livekit::CreateRoomRequest request;
	request.set_name("protobuf-consumer-test");
	(void)api.Room().CreateRoom(request);
}

int main() {
	livekit::CreateRoomRequest request;
	return request.name().empty() ? 0 : 1;
}
