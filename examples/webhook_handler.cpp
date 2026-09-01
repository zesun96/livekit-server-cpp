#include "livekit/server/webhook_receiver.h"

#include <iostream>
#include <string_view>

namespace {

livekit::server::WebhookCallbacks Callbacks() {
	livekit::server::WebhookCallbacks callbacks;
	callbacks.on_participant_joined = [](const livekit::server::WebhookEvent& event) {
		if (event.participant) {
			std::cout << "participant joined: " << event.participant->identity << '\n';
		}
	};
	callbacks.on_egress_ended = [](const livekit::server::WebhookEvent& event) {
		if (event.egress) {
			std::cout << "egress ended: " << event.egress->egress_id << '\n';
		}
	};
	return callbacks;
}

} // namespace

int main() {
	// In an HTTP handler, pass the request body and Authorization header to
	// ReceiveAndDispatch. The receiver verifies both the JWT and body checksum
	// before invoking a callback.
	livekit::server::WebhookReceiver receiver("api-key", "api-secret", Callbacks());
	std::cout << "WebhookReceiver ready; connect it to your HTTP framework\n";
	return 0;
}
