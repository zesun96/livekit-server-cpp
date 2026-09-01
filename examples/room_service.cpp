#include "livekit/server/livekit_api.h"

#include <iostream>

int main() {
	try {
		// URL and credentials fall back to LIVEKIT_URL, LIVEKIT_API_KEY, and
		// LIVEKIT_API_SECRET. Never ship the API secret in a client application.
		livekit::server::LiveKitApi api({});
		const auto rooms = api.Room().ListRooms();
		for (const auto& room : rooms.rooms()) {
			std::cout << room.name() << '\n';
		}
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
