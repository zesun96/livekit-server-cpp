#pragma once

#include <stdexcept>
#include <string>

namespace livekit::server {

enum class ErrorCode {
	invalid_argument,
	authentication,
	transport,
	http,
	protocol,
	unsupported,
};

class Error : public std::runtime_error {
public:
	Error(ErrorCode code, std::string message, int http_status = 0, std::string twirp_code = {});

	[[nodiscard]] ErrorCode code() const noexcept;
	[[nodiscard]] int http_status() const noexcept;
	[[nodiscard]] const std::string& twirp_code() const noexcept;

private:
	ErrorCode code_;
	int http_status_;
	std::string twirp_code_;
};

} // namespace livekit::server
