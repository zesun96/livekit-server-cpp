#pragma once

#include "livekit_agent_dispatch.pb.h"

#include <memory>
#include <optional>
#include <string>

namespace livekit::server {
namespace detail {
class ClientContext;
}

class AgentDispatchClient {
public:
	explicit AgentDispatchClient(std::shared_ptr<detail::ClientContext> context);

	[[nodiscard]] livekit::AgentDispatch
	CreateDispatch(const livekit::CreateAgentDispatchRequest& request) const;
	[[nodiscard]] livekit::AgentDispatch
	DeleteDispatch(const livekit::DeleteAgentDispatchRequest& request) const;
	[[nodiscard]] livekit::ListAgentDispatchResponse
	ListDispatch(const livekit::ListAgentDispatchRequest& request) const;
	[[nodiscard]] std::optional<livekit::AgentDispatch> GetDispatch(std::string dispatch_id,
	                                                                std::string room) const;

private:
	std::shared_ptr<detail::ClientContext> context_;
};

} // namespace livekit::server
