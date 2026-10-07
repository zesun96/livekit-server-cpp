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

class ConnectorClient {
public:
	explicit ConnectorClient(std::shared_ptr<detail::ClientContext> context);

	[[nodiscard]] model::DialWhatsAppCallResponse
	DialWhatsAppCall(const model::DialWhatsAppCallRequest& request) const;
	[[nodiscard]] model::DisconnectWhatsAppCallResponse
	DisconnectWhatsAppCall(const model::DisconnectWhatsAppCallRequest& request) const;
	[[nodiscard]] model::ConnectWhatsAppCallResponse
	ConnectWhatsAppCall(const model::ConnectWhatsAppCallRequest& request) const;
	[[nodiscard]] model::AcceptWhatsAppCallResponse
	AcceptWhatsAppCall(const model::AcceptWhatsAppCallRequest& request) const;
	[[nodiscard]] model::ConnectTwilioCallResponse
	ConnectTwilioCall(const model::ConnectTwilioCallRequest& request) const;

#if defined(LIVEKIT_SERVER_BUILDING_LIBRARY) || defined(LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER_API)
	// Protobuf compatibility overloads are enabled by LiveKitServer::protobuf_adapter.
	[[nodiscard]] livekit::DialWhatsAppCallResponse
	DialWhatsAppCall(const livekit::DialWhatsAppCallRequest& request) const;
	[[nodiscard]] livekit::DisconnectWhatsAppCallResponse
	DisconnectWhatsAppCall(const livekit::DisconnectWhatsAppCallRequest& request) const;
	[[nodiscard]] livekit::ConnectWhatsAppCallResponse
	ConnectWhatsAppCall(const livekit::ConnectWhatsAppCallRequest& request) const;
	[[nodiscard]] livekit::AcceptWhatsAppCallResponse
	AcceptWhatsAppCall(const livekit::AcceptWhatsAppCallRequest& request) const;
	[[nodiscard]] livekit::ConnectTwilioCallResponse
	ConnectTwilioCall(const livekit::ConnectTwilioCallRequest& request) const;
#endif

private:
	std::shared_ptr<detail::ClientContext> context_;
};

} // namespace livekit::server
