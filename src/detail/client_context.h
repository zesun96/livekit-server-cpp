#pragma once

#include "livekit/server/access_token.h"
#include "livekit/server/api_options.h"

#include <google/protobuf/message_lite.h>

#include <memory>
#include <optional>
#include <string>

namespace livekit::server::detail {

struct RequestGrant {
	std::optional<VideoGrant> video;
	std::optional<SipGrant> sip;
	std::optional<AgentGrant> agent;
};

class ClientContext {
public:
	explicit ClientContext(ApiOptions options);

	void Call(const std::string& service, const std::string& method,
	          const google::protobuf::MessageLite& request, google::protobuf::MessageLite* response,
	          const RequestGrant& grant) const;

private:
	[[nodiscard]] std::string TokenFor(const RequestGrant& grant) const;

	std::string url_;
	std::string api_key_;
	std::string api_secret_;
	std::string access_token_;
	std::chrono::milliseconds timeout_;
	std::shared_ptr<HttpTransport> transport_;
};

} // namespace livekit::server::detail
