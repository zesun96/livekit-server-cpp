#include "livekit/server/error.h"

#include <utility>

namespace livekit::server {

Error::Error(ErrorCode code, std::string message, int http_status, std::string twirp_code)
    : std::runtime_error(std::move(message)), code_(code), http_status_(http_status),
      twirp_code_(std::move(twirp_code)) {}

ErrorCode Error::code() const noexcept { return code_; }

int Error::http_status() const noexcept { return http_status_; }

const std::string& Error::twirp_code() const noexcept { return twirp_code_; }

} // namespace livekit::server
