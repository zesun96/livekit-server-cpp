#include <livekit/server/access_token.h>
#include <livekit/server/livekit_api.h>
#include <livekit/server/webhook_receiver.h>

#include <type_traits>

static_assert(std::is_default_constructible_v<livekit::server::WebhookEvent>);

void UsePublicHeadersWithoutGeneratedIncludes() {
	livekit::server::WebhookCallbacks callbacks;
	callbacks.on_room_started = [](const livekit::server::WebhookEvent& event) {
		if (event.room) {
			(void)event.room->name;
		}
	};
}
