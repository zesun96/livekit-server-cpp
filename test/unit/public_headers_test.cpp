#include <livekit/server/access_token.h>
#include <livekit/server/agent_dispatch_client.h>
#include <livekit/server/api_options.h>
#include <livekit/server/connector_client.h>
#include <livekit/server/egress_client.h>
#include <livekit/server/error.h>
#include <livekit/server/http_transport.h>
#include <livekit/server/ingress_client.h>
#include <livekit/server/livekit_api.h>
#include <livekit/server/model/models.h>
#include <livekit/server/model/protocol_model.h>
#include <livekit/server/protocol_fwd.h>
#include <livekit/server/room_service_client.h>
#include <livekit/server/sip_client.h>
#include <livekit/server/webhook_receiver.h>

#include <type_traits>

static_assert(std::is_default_constructible_v<livekit::server::WebhookEvent>);
static_assert(std::is_default_constructible_v<livekit::server::model::CreateRoomRequest>);
static_assert(
    !std::is_same_v<livekit::server::model::CreateRoomRequest, livekit::server::model::Room>);

void UsePublicHeadersWithoutGeneratedIncludes() {
	livekit::server::WebhookCallbacks callbacks;
	auto request = livekit::server::model::CreateRoomRequest::FromJson(R"({"name":"room"})");
	(void)request.Json();
	callbacks.on_room_started = [](const livekit::server::WebhookEvent& event) {
		if (event.room) {
			(void)event.room->name;
		}
	};
}
