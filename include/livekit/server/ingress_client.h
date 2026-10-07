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

class IngressClient {
public:
	explicit IngressClient(std::shared_ptr<detail::ClientContext> context);

	[[nodiscard]] model::IngressInfo
	CreateIngress(const model::CreateIngressRequest& request) const;
	[[nodiscard]] model::IngressInfo
	UpdateIngress(const model::UpdateIngressRequest& request) const;
	[[nodiscard]] model::ListIngressResponse
	ListIngress(const model::ListIngressRequest& request) const;
	[[nodiscard]] model::IngressInfo
	DeleteIngress(const model::DeleteIngressRequest& request) const;

#if defined(LIVEKIT_SERVER_BUILDING_LIBRARY) || defined(LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER_API)
	// Protobuf compatibility overloads are enabled by LiveKitServer::protobuf_adapter.
	[[nodiscard]] livekit::IngressInfo
	CreateIngress(const livekit::CreateIngressRequest& request) const;
	[[nodiscard]] livekit::IngressInfo
	UpdateIngress(const livekit::UpdateIngressRequest& request) const;
	[[nodiscard]] livekit::ListIngressResponse
	ListIngress(const livekit::ListIngressRequest& request) const;
	[[nodiscard]] livekit::IngressInfo
	DeleteIngress(const livekit::DeleteIngressRequest& request) const;
#endif

private:
	std::shared_ptr<detail::ClientContext> context_;
};

} // namespace livekit::server
