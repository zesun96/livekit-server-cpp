#pragma once

#include "livekit/server/model/models.h"

#if defined(LIVEKIT_SERVER_BUILDING_LIBRARY) || defined(LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER_API)
#include "livekit/server/protocol_fwd.h"
#endif

#include <memory>

namespace livekit::server {
namespace detail {
class ClientContext;
}

class SipClient {
public:
	explicit SipClient(std::shared_ptr<detail::ClientContext> context);

	[[nodiscard]] model::SIPInboundTrunkInfo
	CreateInboundTrunk(const model::CreateSIPInboundTrunkRequest& request) const;
	[[nodiscard]] model::SIPOutboundTrunkInfo
	CreateOutboundTrunk(const model::CreateSIPOutboundTrunkRequest& request) const;
	[[nodiscard]] model::SIPInboundTrunkInfo
	UpdateInboundTrunk(const model::UpdateSIPInboundTrunkRequest& request) const;
	[[nodiscard]] model::SIPOutboundTrunkInfo
	UpdateOutboundTrunk(const model::UpdateSIPOutboundTrunkRequest& request) const;
	[[nodiscard]] model::GetSIPInboundTrunkResponse
	GetInboundTrunk(const model::GetSIPInboundTrunkRequest& request) const;
	[[nodiscard]] model::GetSIPOutboundTrunkResponse
	GetOutboundTrunk(const model::GetSIPOutboundTrunkRequest& request) const;
	[[nodiscard]] model::ListSIPTrunkResponse
	ListTrunks(const model::ListSIPTrunkRequest& request) const;
	[[nodiscard]] model::ListSIPInboundTrunkResponse
	ListInboundTrunks(const model::ListSIPInboundTrunkRequest& request) const;
	[[nodiscard]] model::ListSIPOutboundTrunkResponse
	ListOutboundTrunks(const model::ListSIPOutboundTrunkRequest& request) const;
	[[nodiscard]] model::SIPTrunkInfo
	DeleteTrunk(const model::DeleteSIPTrunkRequest& request) const;
	[[nodiscard]] model::SIPDispatchRuleInfo
	CreateDispatchRule(const model::CreateSIPDispatchRuleRequest& request) const;
	[[nodiscard]] model::SIPDispatchRuleInfo
	UpdateDispatchRule(const model::UpdateSIPDispatchRuleRequest& request) const;
	[[nodiscard]] model::ListSIPDispatchRuleResponse
	ListDispatchRules(const model::ListSIPDispatchRuleRequest& request) const;
	[[nodiscard]] model::SIPDispatchRuleInfo
	DeleteDispatchRule(const model::DeleteSIPDispatchRuleRequest& request) const;
	[[nodiscard]] model::SIPParticipantInfo
	CreateParticipant(const model::CreateSIPParticipantRequest& request) const;
	[[nodiscard]] model::Empty
	TransferParticipant(const model::TransferSIPParticipantRequest& request) const;

#if defined(LIVEKIT_SERVER_BUILDING_LIBRARY) || defined(LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER_API)
	// Protobuf compatibility overloads are enabled by LiveKitServer::protobuf_adapter.
	[[nodiscard]] livekit::SIPInboundTrunkInfo
	CreateInboundTrunk(const livekit::CreateSIPInboundTrunkRequest& request) const;
	[[nodiscard]] livekit::SIPOutboundTrunkInfo
	CreateOutboundTrunk(const livekit::CreateSIPOutboundTrunkRequest& request) const;
	[[nodiscard]] livekit::SIPInboundTrunkInfo
	UpdateInboundTrunk(const livekit::UpdateSIPInboundTrunkRequest& request) const;
	[[nodiscard]] livekit::SIPOutboundTrunkInfo
	UpdateOutboundTrunk(const livekit::UpdateSIPOutboundTrunkRequest& request) const;
	[[nodiscard]] livekit::GetSIPInboundTrunkResponse
	GetInboundTrunk(const livekit::GetSIPInboundTrunkRequest& request) const;
	[[nodiscard]] livekit::GetSIPOutboundTrunkResponse
	GetOutboundTrunk(const livekit::GetSIPOutboundTrunkRequest& request) const;
	[[nodiscard]] livekit::ListSIPTrunkResponse
	ListTrunks(const livekit::ListSIPTrunkRequest& request) const;
	[[nodiscard]] livekit::ListSIPInboundTrunkResponse
	ListInboundTrunks(const livekit::ListSIPInboundTrunkRequest& request) const;
	[[nodiscard]] livekit::ListSIPOutboundTrunkResponse
	ListOutboundTrunks(const livekit::ListSIPOutboundTrunkRequest& request) const;
	[[nodiscard]] livekit::SIPTrunkInfo
	DeleteTrunk(const livekit::DeleteSIPTrunkRequest& request) const;
	[[nodiscard]] livekit::SIPDispatchRuleInfo
	CreateDispatchRule(const livekit::CreateSIPDispatchRuleRequest& request) const;
	[[nodiscard]] livekit::SIPDispatchRuleInfo
	UpdateDispatchRule(const livekit::UpdateSIPDispatchRuleRequest& request) const;
	[[nodiscard]] livekit::ListSIPDispatchRuleResponse
	ListDispatchRules(const livekit::ListSIPDispatchRuleRequest& request) const;
	[[nodiscard]] livekit::SIPDispatchRuleInfo
	DeleteDispatchRule(const livekit::DeleteSIPDispatchRuleRequest& request) const;
	[[nodiscard]] livekit::SIPParticipantInfo
	CreateParticipant(const livekit::CreateSIPParticipantRequest& request) const;
	[[nodiscard]] google::protobuf::Empty
	TransferParticipant(const livekit::TransferSIPParticipantRequest& request) const;
#endif

private:
	std::shared_ptr<detail::ClientContext> context_;
};

using SIPClient = SipClient;

} // namespace livekit::server
