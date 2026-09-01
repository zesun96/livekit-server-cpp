#include "livekit/server/access_token.h"

#include "livekit/server/error.h"

#ifdef _WIN32
#include <windows.h>

#include <bcrypt.h>
#endif

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

namespace livekit::server {
namespace {

std::string EscapeJson(const std::string& value) {
	std::ostringstream output;
	for (const unsigned char ch : value) {
		switch (ch) {
		case '"':
			output << "\\\"";
			break;
		case '\\':
			output << "\\\\";
			break;
		case '\b':
			output << "\\b";
			break;
		case '\f':
			output << "\\f";
			break;
		case '\n':
			output << "\\n";
			break;
		case '\r':
			output << "\\r";
			break;
		case '\t':
			output << "\\t";
			break;
		default:
			if (ch < 0x20) {
				output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
				       << static_cast<int>(ch) << std::dec;
			} else {
				output << static_cast<char>(ch);
			}
		}
	}
	return output.str();
}

std::string Base64Url(const std::uint8_t* data, std::size_t size) {
	static constexpr char alphabet[] =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	std::string output;
	output.reserve((size * 4 + 2) / 3);
	for (std::size_t offset = 0; offset < size; offset += 3) {
		const std::uint32_t first = data[offset];
		const std::uint32_t second = offset + 1 < size ? data[offset + 1] : 0;
		const std::uint32_t third = offset + 2 < size ? data[offset + 2] : 0;
		const std::uint32_t value = (first << 16) | (second << 8) | third;
		output.push_back(alphabet[(value >> 18) & 0x3f]);
		output.push_back(alphabet[(value >> 12) & 0x3f]);
		if (offset + 1 < size) {
			output.push_back(alphabet[(value >> 6) & 0x3f]);
		}
		if (offset + 2 < size) {
			output.push_back(alphabet[value & 0x3f]);
		}
	}
	return output;
}

std::string Base64Url(const std::string& value) {
	return Base64Url(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
}

std::vector<std::uint8_t> HmacSha256(const std::string& secret, const std::string& value) {
#ifdef _WIN32
	BCRYPT_ALG_HANDLE algorithm = nullptr;
	BCRYPT_HASH_HANDLE hash = nullptr;
	DWORD object_size = 0;
	DWORD hash_size = 0;
	DWORD copied = 0;
	if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
	                                                BCRYPT_ALG_HANDLE_HMAC_FLAG)) ||
	    !BCRYPT_SUCCESS(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
	                                      reinterpret_cast<PUCHAR>(&object_size),
	                                      sizeof(object_size), &copied, 0)) ||
	    !BCRYPT_SUCCESS(BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
	                                      reinterpret_cast<PUCHAR>(&hash_size), sizeof(hash_size),
	                                      &copied, 0))) {
		if (algorithm != nullptr) {
			BCryptCloseAlgorithmProvider(algorithm, 0);
		}
		throw Error(ErrorCode::authentication, "failed to initialize HS256");
	}

	std::vector<std::uint8_t> object(object_size);
	std::vector<std::uint8_t> digest(hash_size);
	const auto status =
	    BCryptCreateHash(algorithm, &hash, object.data(), static_cast<ULONG>(object.size()),
	                     reinterpret_cast<PUCHAR>(const_cast<char*>(secret.data())),
	                     static_cast<ULONG>(secret.size()), 0);
	if (!BCRYPT_SUCCESS(status) ||
	    !BCRYPT_SUCCESS(BCryptHashData(hash,
	                                   reinterpret_cast<PUCHAR>(const_cast<char*>(value.data())),
	                                   static_cast<ULONG>(value.size()), 0)) ||
	    !BCRYPT_SUCCESS(
	        BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0))) {
		if (hash != nullptr) {
			BCryptDestroyHash(hash);
		}
		BCryptCloseAlgorithmProvider(algorithm, 0);
		throw Error(ErrorCode::authentication, "failed to sign LiveKit access token");
	}
	BCryptDestroyHash(hash);
	BCryptCloseAlgorithmProvider(algorithm, 0);
	return digest;
#else
	(void)secret;
	(void)value;
	throw Error(ErrorCode::unsupported,
	            "HS256 is not available on this platform; provide a pre-signed token");
#endif
}

void AddBool(std::ostringstream& json, bool& first, const char* name, bool value,
             bool include_false = false) {
	if (!value && !include_false) {
		return;
	}
	json << (first ? "" : ",") << '"' << name << "\":" << (value ? "true" : "false");
	first = false;
}

void AddString(std::ostringstream& json, bool& first, const char* name, const std::string& value) {
	if (value.empty()) {
		return;
	}
	json << (first ? "" : ",") << '"' << name << "\":\"" << EscapeJson(value) << '"';
	first = false;
}

std::string VideoGrantJson(const VideoGrant& grant) {
	std::ostringstream json;
	json << '{';
	bool first = true;
	AddBool(json, first, "roomCreate", grant.room_create);
	AddBool(json, first, "roomList", grant.room_list);
	AddBool(json, first, "roomRecord", grant.room_record);
	AddBool(json, first, "roomAdmin", grant.room_admin);
	AddBool(json, first, "roomJoin", grant.room_join);
	AddString(json, first, "room", grant.room);
	AddString(json, first, "destinationRoom", grant.destination_room);
	AddBool(json, first, "ingressAdmin", grant.ingress_admin);
	if (grant.can_publish.has_value()) {
		AddBool(json, first, "canPublish", *grant.can_publish, true);
	}
	if (grant.can_subscribe.has_value()) {
		AddBool(json, first, "canSubscribe", *grant.can_subscribe, true);
	}
	if (grant.can_publish_data.has_value()) {
		AddBool(json, first, "canPublishData", *grant.can_publish_data, true);
	}
	if (grant.can_update_own_metadata.has_value()) {
		AddBool(json, first, "canUpdateOwnMetadata", *grant.can_update_own_metadata, true);
	}
	if (!grant.can_publish_sources.empty()) {
		json << (first ? "" : ",") << "\"canPublishSources\":[";
		for (std::size_t index = 0; index < grant.can_publish_sources.size(); ++index) {
			json << (index == 0 ? "" : ",") << '"' << EscapeJson(grant.can_publish_sources[index])
			     << '"';
		}
		json << ']';
		first = false;
	}
	AddBool(json, first, "hidden", grant.hidden);
	AddBool(json, first, "recorder", grant.recorder);
	AddBool(json, first, "agent", grant.agent);
	json << '}';
	return json.str();
}

} // namespace

AccessToken::AccessToken(std::string api_key, std::string api_secret)
    : api_key_(std::move(api_key)), api_secret_(std::move(api_secret)) {}

AccessToken& AccessToken::SetIdentity(std::string identity) {
	identity_ = std::move(identity);
	return *this;
}

AccessToken& AccessToken::SetName(std::string name) {
	name_ = std::move(name);
	return *this;
}

AccessToken& AccessToken::SetMetadata(std::string metadata) {
	metadata_ = std::move(metadata);
	return *this;
}

AccessToken& AccessToken::SetAttributes(std::map<std::string, std::string> attributes) {
	attributes_ = std::move(attributes);
	return *this;
}

AccessToken& AccessToken::SetValidFor(std::chrono::seconds valid_for) {
	if (valid_for <= std::chrono::seconds::zero()) {
		throw Error(ErrorCode::invalid_argument, "token lifetime must be positive");
	}
	valid_for_ = valid_for;
	return *this;
}

AccessToken& AccessToken::SetVideoGrant(VideoGrant grant) {
	video_grant_ = std::move(grant);
	return *this;
}

AccessToken& AccessToken::SetSipGrant(SipGrant grant) {
	sip_grant_ = grant;
	return *this;
}

AccessToken& AccessToken::SetAgentGrant(AgentGrant grant) {
	agent_grant_ = grant;
	return *this;
}

std::string AccessToken::ToJwt() const { return ToJwt(std::chrono::system_clock::now()); }

std::string AccessToken::ToJwt(std::chrono::system_clock::time_point now) const {
	if (api_key_.empty() || api_secret_.empty()) {
		throw Error(ErrorCode::authentication, "API key and secret are required to sign a token");
	}
	const auto issued_at =
	    std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
	const auto expires_at = issued_at + valid_for_.count();

	std::ostringstream claims;
	claims << "{\"iss\":\"" << EscapeJson(api_key_) << "\",\"nbf\":" << issued_at
	       << ",\"exp\":" << expires_at << ",\"iat\":" << issued_at;
	if (!identity_.empty()) {
		claims << ",\"sub\":\"" << EscapeJson(identity_) << '"';
	}
	if (!name_.empty()) {
		claims << ",\"name\":\"" << EscapeJson(name_) << '"';
	}
	if (!metadata_.empty()) {
		claims << ",\"metadata\":\"" << EscapeJson(metadata_) << '"';
	}
	if (!attributes_.empty()) {
		claims << ",\"attributes\":{";
		bool first = true;
		for (const auto& [key, value] : attributes_) {
			claims << (first ? "" : ",") << '"' << EscapeJson(key) << "\":\"" << EscapeJson(value)
			       << '"';
			first = false;
		}
		claims << '}';
	}
	if (video_grant_.has_value()) {
		claims << ",\"video\":" << VideoGrantJson(*video_grant_);
	}
	if (sip_grant_.has_value()) {
		claims << ",\"sip\":{";
		bool first = true;
		AddBool(claims, first, "admin", sip_grant_->admin);
		AddBool(claims, first, "call", sip_grant_->call);
		claims << '}';
	}
	if (agent_grant_.has_value()) {
		claims << ",\"agent\":{";
		bool first = true;
		AddBool(claims, first, "admin", agent_grant_->admin);
		AddBool(claims, first, "simulationAdmin", agent_grant_->simulation_admin);
		claims << '}';
	}
	claims << '}';

	const std::string header = R"({"alg":"HS256","typ":"JWT"})";
	const std::string unsigned_token = Base64Url(header) + "." + Base64Url(claims.str());
	const auto signature = HmacSha256(api_secret_, unsigned_token);
	return unsigned_token + "." + Base64Url(signature.data(), signature.size());
}

} // namespace livekit::server
