#include "livekit/server/access_token.h"

#include "detail/crypto.h"
#include "livekit/server/error.h"

#include <iomanip>
#include <sstream>
#include <utility>

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
	const std::string unsigned_token =
	    detail::Base64UrlEncode(header) + "." + detail::Base64UrlEncode(claims.str());
	const auto signature = detail::HmacSha256(api_secret_, unsigned_token);
	return unsigned_token + "." + detail::Base64UrlEncode(signature);
}

} // namespace livekit::server
