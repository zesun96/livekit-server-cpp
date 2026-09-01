#include "livekit/server/sip_client.h"

#include "detail/client_context.h"
#include "livekit_sip.pb.h"

#include <google/protobuf/empty.pb.h>

#include <utility>

namespace livekit::server {
namespace {

template <typename Response, typename Request>
Response Call(const std::shared_ptr<detail::ClientContext>& context, const char* method,
              const Request& request, detail::RequestGrant grant) {
	Response response;
	context->Call("SIP", method, request, &response, grant);
	return response;
}

detail::RequestGrant AdminGrant() { return {.sip = SipGrant{.admin = true}}; }

detail::RequestGrant CallGrant(std::string room = {}) {
	detail::RequestGrant grant{.sip = SipGrant{.call = true}};
	if (!room.empty()) {
		grant.video = VideoGrant{.room_admin = true, .room = std::move(room)};
	}
	return grant;
}

} // namespace

SipClient::SipClient(std::shared_ptr<detail::ClientContext> context)
    : context_(std::move(context)) {}

livekit::SIPInboundTrunkInfo
SipClient::CreateInboundTrunk(const livekit::CreateSIPInboundTrunkRequest& request) const {
	return Call<livekit::SIPInboundTrunkInfo>(context_, "CreateSIPInboundTrunk", request,
	                                          AdminGrant());
}

livekit::SIPOutboundTrunkInfo
SipClient::CreateOutboundTrunk(const livekit::CreateSIPOutboundTrunkRequest& request) const {
	return Call<livekit::SIPOutboundTrunkInfo>(context_, "CreateSIPOutboundTrunk", request,
	                                           AdminGrant());
}

livekit::SIPInboundTrunkInfo
SipClient::UpdateInboundTrunk(const livekit::UpdateSIPInboundTrunkRequest& request) const {
	return Call<livekit::SIPInboundTrunkInfo>(context_, "UpdateSIPInboundTrunk", request,
	                                          AdminGrant());
}

livekit::SIPOutboundTrunkInfo
SipClient::UpdateOutboundTrunk(const livekit::UpdateSIPOutboundTrunkRequest& request) const {
	return Call<livekit::SIPOutboundTrunkInfo>(context_, "UpdateSIPOutboundTrunk", request,
	                                           AdminGrant());
}

livekit::GetSIPInboundTrunkResponse
SipClient::GetInboundTrunk(const livekit::GetSIPInboundTrunkRequest& request) const {
	return Call<livekit::GetSIPInboundTrunkResponse>(context_, "GetSIPInboundTrunk", request,
	                                                 AdminGrant());
}

livekit::GetSIPOutboundTrunkResponse
SipClient::GetOutboundTrunk(const livekit::GetSIPOutboundTrunkRequest& request) const {
	return Call<livekit::GetSIPOutboundTrunkResponse>(context_, "GetSIPOutboundTrunk", request,
	                                                  AdminGrant());
}

livekit::ListSIPTrunkResponse
SipClient::ListTrunks(const livekit::ListSIPTrunkRequest& request) const {
	return Call<livekit::ListSIPTrunkResponse>(context_, "ListSIPTrunk", request, AdminGrant());
}

livekit::ListSIPInboundTrunkResponse
SipClient::ListInboundTrunks(const livekit::ListSIPInboundTrunkRequest& request) const {
	return Call<livekit::ListSIPInboundTrunkResponse>(context_, "ListSIPInboundTrunk", request,
	                                                  AdminGrant());
}

livekit::ListSIPOutboundTrunkResponse
SipClient::ListOutboundTrunks(const livekit::ListSIPOutboundTrunkRequest& request) const {
	return Call<livekit::ListSIPOutboundTrunkResponse>(context_, "ListSIPOutboundTrunk", request,
	                                                   AdminGrant());
}

livekit::SIPTrunkInfo SipClient::DeleteTrunk(const livekit::DeleteSIPTrunkRequest& request) const {
	return Call<livekit::SIPTrunkInfo>(context_, "DeleteSIPTrunk", request, AdminGrant());
}

livekit::SIPDispatchRuleInfo
SipClient::CreateDispatchRule(const livekit::CreateSIPDispatchRuleRequest& request) const {
	return Call<livekit::SIPDispatchRuleInfo>(context_, "CreateSIPDispatchRule", request,
	                                          AdminGrant());
}

livekit::SIPDispatchRuleInfo
SipClient::UpdateDispatchRule(const livekit::UpdateSIPDispatchRuleRequest& request) const {
	return Call<livekit::SIPDispatchRuleInfo>(context_, "UpdateSIPDispatchRule", request,
	                                          AdminGrant());
}

livekit::ListSIPDispatchRuleResponse
SipClient::ListDispatchRules(const livekit::ListSIPDispatchRuleRequest& request) const {
	return Call<livekit::ListSIPDispatchRuleResponse>(context_, "ListSIPDispatchRule", request,
	                                                  AdminGrant());
}

livekit::SIPDispatchRuleInfo
SipClient::DeleteDispatchRule(const livekit::DeleteSIPDispatchRuleRequest& request) const {
	return Call<livekit::SIPDispatchRuleInfo>(context_, "DeleteSIPDispatchRule", request,
	                                          AdminGrant());
}

livekit::SIPParticipantInfo
SipClient::CreateParticipant(const livekit::CreateSIPParticipantRequest& request) const {
	return Call<livekit::SIPParticipantInfo>(context_, "CreateSIPParticipant", request,
	                                         CallGrant());
}

google::protobuf::Empty
SipClient::TransferParticipant(const livekit::TransferSIPParticipantRequest& request) const {
	return Call<google::protobuf::Empty>(context_, "TransferSIPParticipant", request,
	                                     CallGrant(request.room_name()));
}

} // namespace livekit::server
