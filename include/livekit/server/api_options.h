#pragma once

#include "livekit/server/http_transport.h"

#include <chrono>
#include <memory>
#include <string>

namespace livekit::server {

struct ApiOptions {
	std::string url;
	std::string api_key;
	std::string api_secret;
	std::string access_token;
	std::chrono::milliseconds timeout{30000};
	std::shared_ptr<HttpTransport> transport;
};

} // namespace livekit::server
