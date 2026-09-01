#include "livekit/server/error.h"
#include "livekit/server/webhook_receiver.h"

#include "detail/crypto.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

void Require(bool condition, const char* message) {
	if (!condition) {
		throw std::runtime_error(message);
	}
}

std::string WebhookToken(std::string_view body, std::chrono::system_clock::time_point now,
                         std::string_view api_key = "key", std::string_view secret = "secret",
                         std::chrono::seconds valid_for = std::chrono::minutes(5),
                         std::chrono::seconds not_before_offset = std::chrono::seconds::zero()) {
	const auto now_seconds =
	    std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
	const auto checksum =
	    livekit::server::detail::Base64Encode(livekit::server::detail::Sha256(body));
	const std::string header = R"({"alg":"HS256","typ":"JWT"})";
	const std::string claims =
	    "{\"iss\":\"" + std::string(api_key) +
	    "\",\"nbf\":" + std::to_string(now_seconds + not_before_offset.count()) +
	    ",\"exp\":" + std::to_string(now_seconds + valid_for.count()) + ",\"sha256\":\"" +
	    checksum + "\"}";
	const auto unsigned_token = livekit::server::detail::Base64UrlEncode(header) + "." +
	                            livekit::server::detail::Base64UrlEncode(claims);
	const auto signature = livekit::server::detail::HmacSha256(secret, unsigned_token);
	return unsigned_token + "." + livekit::server::detail::Base64UrlEncode(signature);
}

void ExpectAuthenticationFailure(const livekit::server::WebhookReceiver& receiver,
                                 std::string_view body, std::string_view token) {
	try {
		(void)receiver.Receive(body, token);
		throw std::runtime_error("expected webhook authentication failure");
	} catch (const livekit::server::Error& error) {
		Require(error.code() == livekit::server::ErrorCode::authentication,
		        "wrong webhook error category");
	}
}

void TestVerifiedDispatch() {
	const std::string body =
	    R"({"event":"participant_joined","room":{"sid":"RM_1","name":"support","metadata":"room-data"},"participant":{"sid":"PA_1","identity":"alice","name":"Alice","metadata":"participant-data"},"track":{"sid":"TR_1","name":"microphone"},"egressInfo":{"egressId":"EG_1","roomId":"RM_1","roomName":"support"},"ingressInfo":{"ingressId":"IN_1","name":"input","roomName":"support"},"id":"EV_1","createdAt":"1700000000","futureField":true})";
	const auto now = std::chrono::system_clock::now();
	int all_events = 0;
	int participant_events = 0;
	livekit::server::WebhookCallbacks callbacks;
	callbacks.on_event = [&](const livekit::server::WebhookEvent& event) {
		++all_events;
		Require(event.id == "EV_1", "generic callback event mismatch");
		Require(event.raw_body == body, "verified raw webhook body was not preserved");
		Require(event.room && event.room->sid == "RM_1" && event.room->name == "support",
		        "room payload mismatch");
		Require(event.track && event.track->sid == "TR_1", "track payload mismatch");
		Require(event.egress && event.egress->egress_id == "EG_1", "egress payload mismatch");
		Require(event.ingress && event.ingress->ingress_id == "IN_1", "ingress payload mismatch");
	};
	callbacks.on_participant_joined = [&](const livekit::server::WebhookEvent& event) {
		++participant_events;
		Require(event.participant.has_value(), "participant payload is missing");
		Require(event.participant->identity == "alice", "participant callback mismatch");
	};
	livekit::server::WebhookReceiver receiver("key", "secret", std::move(callbacks));
	receiver.ReceiveAndDispatch(body, "Bearer " + WebhookToken(body, now));
	Require(all_events == 1, "generic webhook callback was not invoked");
	Require(participant_events == 1, "specific webhook callback was not invoked");
}

void TestTamperingAndUnknownKeys() {
	const std::string body = R"({"event":"room_started","id":"EV_2"})";
	const auto token = WebhookToken(body, std::chrono::system_clock::now());
	livekit::server::WebhookReceiver receiver("key", "secret");
	ExpectAuthenticationFailure(receiver, body + " ", token);

	livekit::server::WebhookReceiver wrong_key("other-key", "secret");
	ExpectAuthenticationFailure(wrong_key, body, token);

	auto bad_signature = token;
	bad_signature.back() = bad_signature.back() == 'A' ? 'B' : 'A';
	ExpectAuthenticationFailure(receiver, body, bad_signature);
}

void TestTokenTimeValidation() {
	const std::string body = R"({"event":"room_finished","id":"EV_3"})";
	const auto now = std::chrono::system_clock::now();
	livekit::server::WebhookReceiver receiver("key", "secret");
	ExpectAuthenticationFailure(receiver, body,
	                            WebhookToken(body, now, "key", "secret", std::chrono::minutes(-2)));
	ExpectAuthenticationFailure(
	    receiver, body,
	    WebhookToken(body, now, "key", "secret", std::chrono::minutes(5), std::chrono::minutes(2)));
}

void TestCallbackReplacementAndUnknownEvent() {
	const std::string body = R"({"event":"future_event","id":"EV_4"})";
	int first = 0;
	int second = 0;
	livekit::server::WebhookCallbacks callbacks;
	callbacks.on_event = [&](const livekit::server::WebhookEvent&) { ++first; };
	livekit::server::WebhookReceiver receiver(
	    [](const std::string& key) { return key == "key" ? "secret" : ""; }, std::move(callbacks));
	receiver.Dispatch(receiver.Receive(body, WebhookToken(body, std::chrono::system_clock::now())));

	livekit::server::WebhookCallbacks replacement;
	replacement.on_event = [&](const livekit::server::WebhookEvent&) { ++second; };
	receiver.SetCallbacks(std::move(replacement));
	receiver.Dispatch(receiver.Receive(body, WebhookToken(body, std::chrono::system_clock::now())));
	Require(first == 1 && second == 1, "webhook callback replacement failed");
}

} // namespace

int main() {
	try {
		TestVerifiedDispatch();
		TestTamperingAndUnknownKeys();
		TestTokenTimeValidation();
		TestCallbackReplacementAndUnknownEvent();
		std::cout << "all webhook tests passed\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "test failure: " << error.what() << '\n';
		return 1;
	}
}
