#include "detail/client_context.h"

#include "livekit/server/error.h"

#ifdef _WIN32
#include <windows.h>

#include <bcrypt.h>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <random>
#include <sstream>
#include <utility>

namespace livekit::server::detail {
namespace {

std::string Environment(const char* name) {
#ifdef _WIN32
	char* value = nullptr;
	std::size_t size = 0;
	if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
		return {};
	}
	std::string result(value);
	std::free(value);
	return result;
#else
	const char* value = std::getenv(name);
	return value != nullptr ? value : "";
#endif
}

bool StartsWithCaseInsensitive(const std::string& value, const std::string& prefix) {
	return value.size() >= prefix.size() &&
	       std::equal(prefix.begin(), prefix.end(), value.begin(), [](char left, char right) {
		       return std::tolower(static_cast<unsigned char>(left)) ==
		              std::tolower(static_cast<unsigned char>(right));
	       });
}

std::string NormalizeUrl(std::string url) {
	if (StartsWithCaseInsensitive(url, "ws://")) {
		url.replace(0, 5, "http://");
	} else if (StartsWithCaseInsensitive(url, "wss://")) {
		url.replace(0, 6, "https://");
	}
	if (!StartsWithCaseInsensitive(url, "http://") && !StartsWithCaseInsensitive(url, "https://")) {
		throw Error(ErrorCode::invalid_argument, "LiveKit URL must use http, https, ws, or wss");
	}
	while (!url.empty() && url.back() == '/') {
		url.pop_back();
	}
	return url;
}

std::string RequestId() {
	std::array<unsigned char, 16> bytes{};
#ifdef _WIN32
	if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
	                                    BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
		throw Error(ErrorCode::transport, "failed to create request id");
	}
#else
	std::random_device random;
	for (auto& byte : bytes) {
		byte = static_cast<unsigned char>(random());
	}
#endif
	bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0f) | 0x40);
	bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3f) | 0x80);
	std::ostringstream value;
	value << std::hex << std::setfill('0');
	for (std::size_t index = 0; index < bytes.size(); ++index) {
		if (index == 4 || index == 6 || index == 8 || index == 10) {
			value << '-';
		}
		value << std::setw(2) << static_cast<unsigned int>(bytes[index]);
	}
	return value.str();
}

std::string JsonStringField(const std::string& json, const std::string& name) {
	const std::string marker = '"' + name + '"';
	auto position = json.find(marker);
	if (position == std::string::npos) {
		return {};
	}
	position = json.find(':', position + marker.size());
	if (position == std::string::npos) {
		return {};
	}
	position = json.find('"', position + 1);
	if (position == std::string::npos) {
		return {};
	}
	std::string result;
	for (++position; position < json.size(); ++position) {
		const char ch = json[position];
		if (ch == '"') {
			return result;
		}
		if (ch == '\\' && position + 1 < json.size()) {
			const char escaped = json[++position];
			switch (escaped) {
			case 'n':
				result.push_back('\n');
				break;
			case 'r':
				result.push_back('\r');
				break;
			case 't':
				result.push_back('\t');
				break;
			default:
				result.push_back(escaped);
			}
		} else {
			result.push_back(ch);
		}
	}
	return {};
}

} // namespace

ClientContext::ClientContext(ApiOptions options)
    : timeout_(options.timeout), transport_(std::move(options.transport)) {
	url_ = options.url.empty() ? Environment("LIVEKIT_URL") : std::move(options.url);
	if (url_.empty()) {
		throw Error(ErrorCode::invalid_argument,
		            "LiveKit URL is required (ApiOptions::url or LIVEKIT_URL)");
	}
	url_ = NormalizeUrl(std::move(url_));

	api_key_ = std::move(options.api_key);
	api_secret_ = std::move(options.api_secret);
	access_token_ = std::move(options.access_token);
	if (access_token_.empty() && api_key_.empty() && api_secret_.empty()) {
		access_token_ = Environment("LIVEKIT_TOKEN");
	}
	if (access_token_.empty() && api_key_.empty() && api_secret_.empty()) {
		api_key_ = Environment("LIVEKIT_API_KEY");
		api_secret_ = Environment("LIVEKIT_API_SECRET");
	}
	if (access_token_.empty() && (api_key_.empty() || api_secret_.empty())) {
		throw Error(ErrorCode::authentication,
		            "provide an access token or both an API key and secret");
	}
	if (timeout_ <= std::chrono::milliseconds::zero()) {
		throw Error(ErrorCode::invalid_argument, "HTTP timeout must be positive");
	}
	if (transport_ == nullptr) {
		transport_ = CreateDefaultHttpTransport();
	}
}

void ClientContext::Call(const std::string& service, const std::string& method,
                         const google::protobuf::MessageLite& request,
                         google::protobuf::MessageLite* response, const RequestGrant& grant) const {
	if (response == nullptr) {
		throw Error(ErrorCode::invalid_argument, "response must not be null");
	}
	HttpRequest http_request;
	http_request.url = url_ + "/twirp/livekit." + service + '/' + method;
	http_request.timeout = timeout_;
	http_request.headers = {{"Content-Type", "application/protobuf"},
	                        {"Accept", "application/protobuf"},
	                        {"Authorization", "Bearer " + TokenFor(grant)},
	                        {"X-Livekit-Request-Id", RequestId()},
	                        {"User-Agent", "livekit-server-cpp/0.1"}};
	if (!request.SerializeToString(&http_request.body)) {
		throw Error(ErrorCode::protocol, "failed to serialize protobuf request");
	}

	const auto http_response = transport_->Send(http_request);
	if (http_response.status_code < 200 || http_response.status_code >= 300) {
		const auto twirp_code = JsonStringField(http_response.body, "code");
		auto message = JsonStringField(http_response.body, "msg");
		if (message.empty()) {
			message = "LiveKit server returned HTTP " + std::to_string(http_response.status_code);
		}
		throw Error(ErrorCode::http, std::move(message), http_response.status_code, twirp_code);
	}
	if (!response->ParseFromString(http_response.body)) {
		throw Error(ErrorCode::protocol, "failed to parse protobuf response");
	}
}

std::string ClientContext::TokenFor(const RequestGrant& grant) const {
	if (!access_token_.empty()) {
		return access_token_;
	}
	AccessToken token(api_key_, api_secret_);
	if (grant.video.has_value()) {
		token.SetVideoGrant(*grant.video);
	}
	if (grant.sip.has_value()) {
		token.SetSipGrant(*grant.sip);
	}
	if (grant.agent.has_value()) {
		token.SetAgentGrant(*grant.agent);
	}
	return token.ToJwt();
}

} // namespace livekit::server::detail
