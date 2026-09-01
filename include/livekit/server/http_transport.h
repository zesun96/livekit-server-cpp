#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace livekit::server {

using HttpHeaders = std::vector<std::pair<std::string, std::string>>;

struct HttpRequest {
	std::string url;
	std::string method{"POST"};
	HttpHeaders headers;
	std::string body;
	std::chrono::milliseconds timeout{30000};
};

struct HttpResponse {
	int status_code{};
	HttpHeaders headers;
	std::string body;
};

class HttpTransport {
public:
	virtual ~HttpTransport() = default;
	virtual HttpResponse Send(const HttpRequest& request) = 0;
};

// Uses WinHTTP on Windows. Other platforms can provide an application-owned
// HttpTransport until a native backend is added.
std::shared_ptr<HttpTransport> CreateDefaultHttpTransport();

} // namespace livekit::server
