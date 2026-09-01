#include "livekit/server/ingress_client.h"

#include "detail/client_context.h"

#include <utility>

namespace livekit::server {
namespace {

template <typename Response, typename Request>
Response Call(const std::shared_ptr<detail::ClientContext>& context, const char* method,
              const Request& request) {
	Response response;
	detail::RequestGrant grant{.video = VideoGrant{.ingress_admin = true}};
	context->Call("Ingress", method, request, &response, grant);
	return response;
}

} // namespace

IngressClient::IngressClient(std::shared_ptr<detail::ClientContext> context)
    : context_(std::move(context)) {}

livekit::IngressInfo
IngressClient::CreateIngress(const livekit::CreateIngressRequest& request) const {
	return Call<livekit::IngressInfo>(context_, "CreateIngress", request);
}

livekit::IngressInfo
IngressClient::UpdateIngress(const livekit::UpdateIngressRequest& request) const {
	return Call<livekit::IngressInfo>(context_, "UpdateIngress", request);
}

livekit::ListIngressResponse
IngressClient::ListIngress(const livekit::ListIngressRequest& request) const {
	return Call<livekit::ListIngressResponse>(context_, "ListIngress", request);
}

livekit::IngressInfo
IngressClient::DeleteIngress(const livekit::DeleteIngressRequest& request) const {
	return Call<livekit::IngressInfo>(context_, "DeleteIngress", request);
}

} // namespace livekit::server
