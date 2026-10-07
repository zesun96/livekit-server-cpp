#include <livekit/server/livekit_api.h>

#include <livekit_agent_dispatch.pb.h>
#include <livekit_connector.pb.h>
#include <livekit_egress.pb.h>
#include <livekit_ingress.pb.h>
#include <livekit_room.pb.h>
#include <livekit_sip.pb.h>

void UseOptionalProtobufAdapterHeaders() {
	livekit::CreateRoomRequest room;
	livekit::ListEgressRequest egress;
	livekit::ListIngressRequest ingress;
	livekit::ListSIPTrunkRequest sip;
	livekit::ListAgentDispatchRequest dispatch;
	livekit::DisconnectWhatsAppCallRequest connector;
	(void)room;
	(void)egress;
	(void)ingress;
	(void)sip;
	(void)dispatch;
	(void)connector;
}
