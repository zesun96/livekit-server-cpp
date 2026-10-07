#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace livekit::server {

enum class ErrorCode : std::uint8_t {
	invalid_argument = 0,
	authentication = 1,
	transport = 2,
	http = 3,
	protocol = 4,
	unsupported = 5,
	unknown = 255,
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
