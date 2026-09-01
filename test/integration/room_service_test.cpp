#include "livekit/server/livekit_api.h"
#include "livekit_room.pb.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
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

int main() {
	if (!HasEnvironment("LIVEKIT_URL") ||
	    (!HasEnvironment("LIVEKIT_TOKEN") &&
	     (!HasEnvironment("LIVEKIT_API_KEY") || !HasEnvironment("LIVEKIT_API_SECRET")))) {
		std::cout << "skipped: set LIVEKIT_URL and LiveKit credentials\n";
		return 77;
	}

	const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
	const std::string room_name = "cpp-server-sdk-test-" + std::to_string(suffix);
	try {
		livekit::server::LiveKitApi api({});
		livekit::CreateRoomRequest create;
		create.set_name(room_name);
		const auto room = api.Room().CreateRoom(create);
		if (room.name() != room_name) {
			std::cerr << "created room name mismatch\n";
			return 1;
		}
		livekit::DeleteRoomRequest remove;
		remove.set_room(room_name);
		(void)api.Room().DeleteRoom(remove);
		std::cout << "room service integration test passed\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
