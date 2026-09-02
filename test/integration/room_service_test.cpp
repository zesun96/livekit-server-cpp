#include "livekit/server/livekit_api.h"
#include "livekit_room.pb.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <string>

namespace {

bool HasEnvironment(const char* name) {
#ifdef _WIN32
	char* value = nullptr;
	std::size_t size = 0;
	const bool present = _dupenv_s(&value, &size, name) == 0 && value != nullptr && *value != '\0';
	std::free(value);
	return present;
#else
	const char* value = std::getenv(name);
	return value != nullptr && *value != '\0';
#endif
}

} // namespace

TEST(RoomServiceIntegrationTest, CreatesAndDeletesRoom) {
	if (!HasEnvironment("LIVEKIT_URL") ||
	    (!HasEnvironment("LIVEKIT_TOKEN") &&
	     (!HasEnvironment("LIVEKIT_API_KEY") || !HasEnvironment("LIVEKIT_API_SECRET")))) {
		GTEST_SKIP() << "set LIVEKIT_URL and LiveKit credentials";
	}

	const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
	const std::string room_name = "cpp-server-sdk-test-" + std::to_string(suffix);
	livekit::server::LiveKitApi api({});
	livekit::CreateRoomRequest create;
	create.set_name(room_name);
	const auto room = api.Room().CreateRoom(create);
	EXPECT_EQ(room.name(), room_name);
	livekit::DeleteRoomRequest remove;
	remove.set_room(room_name);
	(void)api.Room().DeleteRoom(remove);
}
