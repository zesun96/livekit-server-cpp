#pragma once

#include "livekit/server/model/models.h"

#if defined(LIVEKIT_SERVER_BUILDING_LIBRARY) || defined(LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER_API)
#include "livekit/server/protocol_fwd.h"
#endif

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

	[[nodiscard]] model::AgentDispatch
	CreateDispatch(const model::CreateAgentDispatchRequest& request) const;
	[[nodiscard]] model::AgentDispatch
	DeleteDispatch(const model::DeleteAgentDispatchRequest& request) const;
	[[nodiscard]] model::ListAgentDispatchResponse
	ListDispatch(const model::ListAgentDispatchRequest& request) const;
	[[nodiscard]] std::optional<model::AgentDispatch> GetDispatchModel(std::string dispatch_id,
	                                                                   std::string room) const;

#if defined(LIVEKIT_SERVER_BUILDING_LIBRARY) || defined(LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER_API)
	// Protobuf compatibility overloads are enabled by LiveKitServer::protobuf_adapter.
	[[nodiscard]] livekit::AgentDispatch
	CreateDispatch(const livekit::CreateAgentDispatchRequest& request) const;
	[[nodiscard]] livekit::AgentDispatch
	DeleteDispatch(const livekit::DeleteAgentDispatchRequest& request) const;
	[[nodiscard]] livekit::ListAgentDispatchResponse
	ListDispatch(const livekit::ListAgentDispatchRequest& request) const;
	[[nodiscard]] std::shared_ptr<livekit::AgentDispatch> GetDispatch(std::string dispatch_id,
	                                                                  std::string room) const;
#endif

private:
	std::shared_ptr<detail::ClientContext> context_;
};

} // namespace livekit::server
