#pragma once

#include "livekit_sip.pb.h"

#include <google/protobuf/empty.pb.h>

#include <memory>

namespace livekit::server {
namespace detail {
class ClientContext;
}

class SipClient {
public:
	explicit SipClient(std::shared_ptr<detail::ClientContext> context);

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

private:
	std::shared_ptr<detail::ClientContext> context_;
};

using SIPClient = SipClient;

} // namespace livekit::server
