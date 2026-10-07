#include "livekit/server/connector_client.h"

#include "detail/client_context.h"
#include "detail/proto/json_codec.h"
#include "livekit_connector.pb.h"

#include <utility>

namespace livekit::server {
namespace {

template <typename Response, typename Request>
Response Call(const std::shared_ptr<detail::ClientContext>& context, const char* method,
              const Request& request, std::string room = {}) {
	Response response;
	VideoGrant video;
	video.room_create = true;
	video.room = std::move(room);
	detail::RequestGrant grant{.video = std::move(video)};
	context->Call("Connector", method, request, &response, grant);
	return response;
}

} // namespace

ConnectorClient::ConnectorClient(std::shared_ptr<detail::ClientContext> context)
    : context_(std::move(context)) {}

#define LIVEKIT_CONNECTOR_MODEL_METHOD(response, method, request, proto_response,                  \
                                       proto_request_type)                                         \
	model::response ConnectorClient::method(const model::request& request) const {                 \
		auto protobuf_request = detail::proto::FromModel<livekit::proto_request_type>(request);    \
		return detail::proto::ToModel<model::response>(method(protobuf_request));                  \
	}

LIVEKIT_CONNECTOR_MODEL_METHOD(DialWhatsAppCallResponse, DialWhatsAppCall, DialWhatsAppCallRequest,
                               DialWhatsAppCallResponse, DialWhatsAppCallRequest)
LIVEKIT_CONNECTOR_MODEL_METHOD(DisconnectWhatsAppCallResponse, DisconnectWhatsAppCall,
                               DisconnectWhatsAppCallRequest, DisconnectWhatsAppCallResponse,
                               DisconnectWhatsAppCallRequest)
LIVEKIT_CONNECTOR_MODEL_METHOD(ConnectWhatsAppCallResponse, ConnectWhatsAppCall,
                               ConnectWhatsAppCallRequest, ConnectWhatsAppCallResponse,
                               ConnectWhatsAppCallRequest)
LIVEKIT_CONNECTOR_MODEL_METHOD(AcceptWhatsAppCallResponse, AcceptWhatsAppCall,
                               AcceptWhatsAppCallRequest, AcceptWhatsAppCallResponse,
                               AcceptWhatsAppCallRequest)
LIVEKIT_CONNECTOR_MODEL_METHOD(ConnectTwilioCallResponse, ConnectTwilioCall,
                               ConnectTwilioCallRequest, ConnectTwilioCallResponse,
                               ConnectTwilioCallRequest)

#undef LIVEKIT_CONNECTOR_MODEL_METHOD

livekit::DialWhatsAppCallResponse
ConnectorClient::DialWhatsAppCall(const livekit::DialWhatsAppCallRequest& request) const {
	return Call<livekit::DialWhatsAppCallResponse>(context_, "DialWhatsAppCall", request,
	                                               request.room_name());
}

livekit::DisconnectWhatsAppCallResponse ConnectorClient::DisconnectWhatsAppCall(
    const livekit::DisconnectWhatsAppCallRequest& request) const {
	return Call<livekit::DisconnectWhatsAppCallResponse>(context_, "DisconnectWhatsAppCall",
	                                                     request);
}

livekit::ConnectWhatsAppCallResponse
ConnectorClient::ConnectWhatsAppCall(const livekit::ConnectWhatsAppCallRequest& request) const {
	return Call<livekit::ConnectWhatsAppCallResponse>(context_, "ConnectWhatsAppCall", request);
}

livekit::AcceptWhatsAppCallResponse
ConnectorClient::AcceptWhatsAppCall(const livekit::AcceptWhatsAppCallRequest& request) const {
	return Call<livekit::AcceptWhatsAppCallResponse>(context_, "AcceptWhatsAppCall", request,
	                                                 request.room_name());
}

livekit::ConnectTwilioCallResponse
ConnectorClient::ConnectTwilioCall(const livekit::ConnectTwilioCallRequest& request) const {
	return Call<livekit::ConnectTwilioCallResponse>(context_, "ConnectTwilioCall", request,
	                                                request.room_name());
}

} // namespace livekit::server
