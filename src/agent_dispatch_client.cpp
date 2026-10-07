#include "livekit/server/agent_dispatch_client.h"

#include "detail/client_context.h"
#include "detail/proto/json_codec.h"
#include "livekit_agent_dispatch.pb.h"

#include <utility>

namespace livekit::server {
namespace {

template <typename Response, typename Request>
Response Call(const std::shared_ptr<detail::ClientContext>& context, const char* method,
              const Request& request, const std::string& room) {
	Response response;
	detail::RequestGrant grant{
	    .video = VideoGrant{.room_admin = true, .room = room},
	};
	context->Call("AgentDispatchService", method, request, &response, grant);
	return response;
}

} // namespace

AgentDispatchClient::AgentDispatchClient(std::shared_ptr<detail::ClientContext> context)
    : context_(std::move(context)) {}

#define LIVEKIT_AGENT_MODEL_METHOD(response, method, request, proto_response, proto_request_type)  \
	model::response AgentDispatchClient::method(const model::request& request) const {             \
		auto protobuf_request = detail::proto::FromModel<livekit::proto_request_type>(request);    \
		return detail::proto::ToModel<model::response>(method(protobuf_request));                  \
	}

LIVEKIT_AGENT_MODEL_METHOD(AgentDispatch, CreateDispatch, CreateAgentDispatchRequest, AgentDispatch,
                           CreateAgentDispatchRequest)
LIVEKIT_AGENT_MODEL_METHOD(AgentDispatch, DeleteDispatch, DeleteAgentDispatchRequest, AgentDispatch,
                           DeleteAgentDispatchRequest)
LIVEKIT_AGENT_MODEL_METHOD(ListAgentDispatchResponse, ListDispatch, ListAgentDispatchRequest,
                           ListAgentDispatchResponse, ListAgentDispatchRequest)

#undef LIVEKIT_AGENT_MODEL_METHOD

std::optional<model::AgentDispatch> AgentDispatchClient::GetDispatchModel(std::string dispatch_id,
                                                                          std::string room) const {
	livekit::ListAgentDispatchRequest request;
	request.set_dispatch_id(std::move(dispatch_id));
	request.set_room(std::move(room));
	auto response = ListDispatch(request);
	if (response.agent_dispatches().empty()) {
		return std::nullopt;
	}
	return detail::proto::ToModel<model::AgentDispatch>(response.agent_dispatches(0));
}

livekit::AgentDispatch
AgentDispatchClient::CreateDispatch(const livekit::CreateAgentDispatchRequest& request) const {
	return Call<livekit::AgentDispatch>(context_, "CreateDispatch", request, request.room());
}

livekit::AgentDispatch
AgentDispatchClient::DeleteDispatch(const livekit::DeleteAgentDispatchRequest& request) const {
	return Call<livekit::AgentDispatch>(context_, "DeleteDispatch", request, request.room());
}

livekit::ListAgentDispatchResponse
AgentDispatchClient::ListDispatch(const livekit::ListAgentDispatchRequest& request) const {
	return Call<livekit::ListAgentDispatchResponse>(context_, "ListDispatch", request,
	                                                request.room());
}

std::shared_ptr<livekit::AgentDispatch> AgentDispatchClient::GetDispatch(std::string dispatch_id,
                                                                         std::string room) const {
	livekit::ListAgentDispatchRequest request;
	request.set_dispatch_id(std::move(dispatch_id));
	request.set_room(std::move(room));
	auto response = ListDispatch(request);
	if (response.agent_dispatches().empty()) {
		return {};
	}
	return std::make_shared<livekit::AgentDispatch>(response.agent_dispatches(0));
}

} // namespace livekit::server
