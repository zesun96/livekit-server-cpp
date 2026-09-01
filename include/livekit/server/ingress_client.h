#pragma once

#include "livekit_ingress.pb.h"

#include <memory>

namespace livekit::server {
namespace detail {
class ClientContext;
}

class IngressClient {
public:
	explicit IngressClient(std::shared_ptr<detail::ClientContext> context);

	[[nodiscard]] livekit::IngressInfo
	CreateIngress(const livekit::CreateIngressRequest& request) const;
	[[nodiscard]] livekit::IngressInfo
	UpdateIngress(const livekit::UpdateIngressRequest& request) const;
	[[nodiscard]] livekit::ListIngressResponse
	ListIngress(const livekit::ListIngressRequest& request) const;
	[[nodiscard]] livekit::IngressInfo
	DeleteIngress(const livekit::DeleteIngressRequest& request) const;

private:
	std::shared_ptr<detail::ClientContext> context_;
};

} // namespace livekit::server
