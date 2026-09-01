#pragma once

#include "livekit/server/protocol_fwd.h"

#include <memory>

namespace livekit::server {
namespace detail {
class ClientContext;
}

class ConnectorClient {
public:
	explicit ConnectorClient(std::shared_ptr<detail::ClientContext> context);

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

private:
	std::shared_ptr<detail::ClientContext> context_;
};

} // namespace livekit::server
