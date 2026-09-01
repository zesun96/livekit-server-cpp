#include "livekit/server/livekit_api.h"

#include "detail/client_context.h"

#include <utility>

namespace livekit::server {

LiveKitApi::LiveKitApi(ApiOptions options)
    : context_(std::make_shared<detail::ClientContext>(std::move(options))), room_(context_),
      egress_(context_), ingress_(context_), sip_(context_), agent_dispatch_(context_),
      connector_(context_) {}

RoomServiceClient& LiveKitApi::Room() noexcept { return room_; }

EgressClient& LiveKitApi::Egress() noexcept { return egress_; }

IngressClient& LiveKitApi::Ingress() noexcept { return ingress_; }

SipClient& LiveKitApi::SIP() noexcept { return sip_; }

SipClient& LiveKitApi::Sip() noexcept { return sip_; }

AgentDispatchClient& LiveKitApi::AgentDispatch() noexcept { return agent_dispatch_; }

ConnectorClient& LiveKitApi::Connector() noexcept { return connector_; }

} // namespace livekit::server
