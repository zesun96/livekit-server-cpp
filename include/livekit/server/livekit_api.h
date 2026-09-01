#pragma once

#include "livekit/server/agent_dispatch_client.h"
#include "livekit/server/api_options.h"
#include "livekit/server/connector_client.h"
#include "livekit/server/egress_client.h"
#include "livekit/server/ingress_client.h"
#include "livekit/server/room_service_client.h"
#include "livekit/server/sip_client.h"
#include "livekit/server/webhook_receiver.h"

#include <memory>

namespace livekit::server {
namespace detail {
class ClientContext;
}

class LiveKitApi {
public:
	explicit LiveKitApi(ApiOptions options);

	LiveKitApi(const LiveKitApi&) = delete;
	LiveKitApi& operator=(const LiveKitApi&) = delete;
	LiveKitApi(LiveKitApi&&) noexcept = default;
	LiveKitApi& operator=(LiveKitApi&&) noexcept = default;

	[[nodiscard]] RoomServiceClient& Room() noexcept;
	[[nodiscard]] EgressClient& Egress() noexcept;
	[[nodiscard]] IngressClient& Ingress() noexcept;
	[[nodiscard]] SipClient& SIP() noexcept;
	[[nodiscard]] SipClient& Sip() noexcept;
	[[nodiscard]] AgentDispatchClient& AgentDispatch() noexcept;
	[[nodiscard]] ConnectorClient& Connector() noexcept;

private:
	std::shared_ptr<detail::ClientContext> context_;
	RoomServiceClient room_;
	EgressClient egress_;
	IngressClient ingress_;
	SipClient sip_;
	AgentDispatchClient agent_dispatch_;
	ConnectorClient connector_;
};

using LiveKitAPI = LiveKitApi;

} // namespace livekit::server
