#include "livekit/server/webhook_receiver.h"

#include "detail/crypto.h"
#include "livekit/server/error.h"
#include "livekit_webhook.pb.h"

#include <google/protobuf/util/json_util.h>

#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

namespace livekit::server {
namespace {

constexpr auto token_leeway = std::chrono::minutes(1);

std::string JsonStringField(std::string_view json, std::string_view name) {
	const std::string marker = '"' + std::string(name) + '"';
	auto position = json.find(marker);
	if (position == std::string_view::npos) {
		return {};
	}
	position = json.find(':', position + marker.size());
	if (position == std::string_view::npos) {
		return {};
	}
	position = json.find('"', position + 1);
	if (position == std::string_view::npos) {
		return {};
	}
	std::string result;
	for (++position; position < json.size(); ++position) {
		const char ch = json[position];
		if (ch == '"') {
			return result;
		}
		if (ch != '\\' || position + 1 >= json.size()) {
			result.push_back(ch);
			continue;
		}
		const char escaped = json[++position];
		switch (escaped) {
		case '"':
		case '\\':
		case '/':
			result.push_back(escaped);
			break;
		case 'b':
			result.push_back('\b');
			break;
		case 'f':
			result.push_back('\f');
			break;
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
			throw Error(ErrorCode::authentication, "webhook JWT contains invalid JSON");
		}
	}
	throw Error(ErrorCode::authentication, "webhook JWT contains unterminated JSON");
}

std::optional<std::int64_t> JsonIntegerField(std::string_view json, std::string_view name) {
	const std::string marker = '"' + std::string(name) + '"';
	auto position = json.find(marker);
	if (position == std::string_view::npos) {
		return std::nullopt;
	}
	position = json.find(':', position + marker.size());
	if (position == std::string_view::npos) {
		throw Error(ErrorCode::authentication, "webhook JWT contains invalid JSON");
	}
	++position;
	while (position < json.size() &&
	       std::isspace(static_cast<unsigned char>(json[position])) != 0) {
		++position;
	}
	bool negative = false;
	if (position < json.size() && json[position] == '-') {
		negative = true;
		++position;
	}
	if (position >= json.size() || std::isdigit(static_cast<unsigned char>(json[position])) == 0) {
		throw Error(ErrorCode::authentication, "webhook JWT time claim is invalid");
	}
	std::int64_t value = 0;
	while (position < json.size() &&
	       std::isdigit(static_cast<unsigned char>(json[position])) != 0) {
		const int digit = json[position++] - '0';
		if (value > (std::numeric_limits<std::int64_t>::max() - digit) / 10) {
			throw Error(ErrorCode::authentication, "webhook JWT time claim is too large");
		}
		value = value * 10 + digit;
	}
	return negative ? -value : value;
}

std::string BytesToString(const std::vector<std::uint8_t>& bytes) {
	return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

WebhookCallback SpecificCallback(const WebhookCallbacks& callbacks, std::string_view event) {
	if (event == webhook_event::room_started) {
		return callbacks.on_room_started;
	}
	if (event == webhook_event::room_finished) {
		return callbacks.on_room_finished;
	}
	if (event == webhook_event::participant_joined) {
		return callbacks.on_participant_joined;
	}
	if (event == webhook_event::participant_left) {
		return callbacks.on_participant_left;
	}
	if (event == webhook_event::participant_connection_aborted) {
		return callbacks.on_participant_connection_aborted;
	}
	if (event == webhook_event::track_published) {
		return callbacks.on_track_published;
	}
	if (event == webhook_event::track_unpublished) {
		return callbacks.on_track_unpublished;
	}
	if (event == webhook_event::egress_started) {
		return callbacks.on_egress_started;
	}
	if (event == webhook_event::egress_updated) {
		return callbacks.on_egress_updated;
	}
	if (event == webhook_event::egress_ended) {
		return callbacks.on_egress_ended;
	}
	if (event == webhook_event::ingress_started) {
		return callbacks.on_ingress_started;
	}
	if (event == webhook_event::ingress_ended) {
		return callbacks.on_ingress_ended;
	}
	return {};
}

} // namespace

WebhookReceiver::WebhookReceiver(std::string api_key, std::string api_secret,
                                 WebhookCallbacks callbacks)
    : WebhookReceiver(
          [key = std::move(api_key), secret = std::move(api_secret)](const std::string& requested) {
	          return requested == key ? secret : std::string{};
          },
          std::move(callbacks)) {}

WebhookReceiver::WebhookReceiver(std::map<std::string, std::string> api_secrets,
                                 WebhookCallbacks callbacks)
    : WebhookReceiver(
          [secrets = std::move(api_secrets)](const std::string& requested) {
	          const auto found = secrets.find(requested);
	          return found == secrets.end() ? std::string{} : found->second;
          },
          std::move(callbacks)) {}

WebhookReceiver::WebhookReceiver(WebhookKeyProvider key_provider, WebhookCallbacks callbacks)
    : key_provider_(std::move(key_provider)), callbacks_(std::move(callbacks)) {
	if (!key_provider_) {
		throw Error(ErrorCode::invalid_argument, "webhook key provider is required");
	}
}

void WebhookReceiver::SetCallbacks(WebhookCallbacks callbacks) {
	std::lock_guard lock(callbacks_mutex_);
	callbacks_ = std::move(callbacks);
}

WebhookEvent WebhookReceiver::Receive(std::string_view body, std::string_view authorization) const {
	return ReceiveAt(body, authorization, std::chrono::system_clock::now());
}

WebhookEvent WebhookReceiver::ReceiveAt(std::string_view body, std::string_view authorization,
                                        std::chrono::system_clock::time_point now) const {
	if (authorization.starts_with("Bearer ")) {
		authorization.remove_prefix(7);
	}
	if (authorization.empty()) {
		throw Error(ErrorCode::authentication, "webhook Authorization header is required");
	}
	const auto first_dot = authorization.find('.');
	const auto second_dot = authorization.find('.', first_dot + 1);
	if (first_dot == std::string_view::npos || second_dot == std::string_view::npos ||
	    authorization.find('.', second_dot + 1) != std::string_view::npos) {
		throw Error(ErrorCode::authentication, "webhook Authorization header is not a JWT");
	}
	const auto header = BytesToString(detail::Base64UrlDecode(authorization.substr(0, first_dot)));
	if (JsonStringField(header, "alg") != "HS256") {
		throw Error(ErrorCode::authentication, "webhook JWT must use HS256");
	}
	const auto claims = BytesToString(
	    detail::Base64UrlDecode(authorization.substr(first_dot + 1, second_dot - first_dot - 1)));
	const auto issuer = JsonStringField(claims, "iss");
	if (issuer.empty()) {
		throw Error(ErrorCode::authentication, "webhook JWT issuer is missing");
	}
	const auto secret = key_provider_(issuer);
	if (secret.empty()) {
		throw Error(ErrorCode::authentication, "webhook API secret was not found");
	}
	const auto expected_signature = detail::HmacSha256(secret, authorization.substr(0, second_dot));
	const auto actual_signature = detail::Base64UrlDecode(authorization.substr(second_dot + 1));
	if (!detail::ConstantTimeEqual(expected_signature, actual_signature)) {
		throw Error(ErrorCode::authentication, "webhook JWT signature is invalid");
	}

	const auto now_seconds =
	    std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
	const auto leeway_seconds =
	    std::chrono::duration_cast<std::chrono::seconds>(token_leeway).count();
	if (const auto expires = JsonIntegerField(claims, "exp");
	    expires.has_value() && now_seconds > *expires + leeway_seconds) {
		throw Error(ErrorCode::authentication, "webhook JWT has expired");
	}
	if (const auto not_before = JsonIntegerField(claims, "nbf");
	    not_before.has_value() && now_seconds + leeway_seconds < *not_before) {
		throw Error(ErrorCode::authentication, "webhook JWT is not valid yet");
	}

	const auto claimed_checksum = JsonStringField(claims, "sha256");
	const auto actual_checksum = detail::Base64Encode(detail::Sha256(body));
	if (claimed_checksum.empty() || !detail::ConstantTimeEqual(claimed_checksum, actual_checksum)) {
		throw Error(ErrorCode::authentication, "webhook body checksum is invalid");
	}

	livekit::WebhookEvent parsed;
	google::protobuf::util::JsonParseOptions options;
	options.ignore_unknown_fields = true;
	const auto status =
	    google::protobuf::util::JsonStringToMessage(std::string(body), &parsed, options);
	if (!status.ok()) {
		throw Error(ErrorCode::protocol, "webhook JSON is invalid: " + status.ToString());
	}

	WebhookEvent event;
	event.event = parsed.event();
	event.id = parsed.id();
	event.created_at = parsed.created_at();
	event.raw_body = body;
	if (parsed.has_room()) {
		event.room =
		    WebhookRoom{parsed.room().sid(), parsed.room().name(), parsed.room().metadata()};
	}
	if (parsed.has_participant()) {
		event.participant =
		    WebhookParticipant{parsed.participant().sid(), parsed.participant().identity(),
		                       parsed.participant().name(), parsed.participant().metadata()};
	}
	if (parsed.has_track()) {
		event.track = WebhookTrack{parsed.track().sid(), parsed.track().name()};
	}
	if (parsed.has_egress_info()) {
		event.egress =
		    WebhookEgress{parsed.egress_info().egress_id(), parsed.egress_info().room_id(),
		                  parsed.egress_info().room_name()};
	}
	if (parsed.has_ingress_info()) {
		event.ingress =
		    WebhookIngress{parsed.ingress_info().ingress_id(), parsed.ingress_info().name(),
		                   parsed.ingress_info().room_name()};
	}
	return event;
}

void WebhookReceiver::Dispatch(const WebhookEvent& event) const {
	WebhookCallbacks callbacks;
	{
		std::lock_guard lock(callbacks_mutex_);
		callbacks = callbacks_;
	}
	if (callbacks.on_event) {
		callbacks.on_event(event);
	}
	if (auto callback = SpecificCallback(callbacks, event.event)) {
		callback(event);
	}
}

void WebhookReceiver::ReceiveAndDispatch(std::string_view body,
                                         std::string_view authorization) const {
	Dispatch(Receive(body, authorization));
}

} // namespace livekit::server
