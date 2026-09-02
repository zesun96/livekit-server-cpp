#include "livekit/server/error.h"
#include "livekit/server/webhook_receiver.h"

#include "detail/crypto.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <string_view>
#include <utility>

namespace {

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
		FAIL() << "expected webhook authentication failure";
	} catch (const livekit::server::Error& error) {
		EXPECT_EQ(error.code(), livekit::server::ErrorCode::authentication);
	}
}

TEST(WebhookReceiverTest, VerifiesAndDispatchesTypedPayload) {
	const std::string body =
	    R"({"event":"participant_joined","room":{"sid":"RM_1","name":"support","metadata":"room-data"},"participant":{"sid":"PA_1","identity":"alice","name":"Alice","metadata":"participant-data"},"track":{"sid":"TR_1","name":"microphone"},"egressInfo":{"egressId":"EG_1","roomId":"RM_1","roomName":"support"},"ingressInfo":{"ingressId":"IN_1","name":"input","roomName":"support"},"id":"EV_1","createdAt":"1700000000","futureField":true})";
	const auto now = std::chrono::system_clock::now();
	int all_events = 0;
	int participant_events = 0;
	livekit::server::WebhookCallbacks callbacks;
	callbacks.on_event = [&](const livekit::server::WebhookEvent& event) {
		++all_events;
		EXPECT_EQ(event.id, "EV_1");
		EXPECT_EQ(event.raw_body, body);
		ASSERT_TRUE(event.room);
		EXPECT_EQ(event.room->sid, "RM_1");
		EXPECT_EQ(event.room->name, "support");
		ASSERT_TRUE(event.track);
		EXPECT_EQ(event.track->sid, "TR_1");
		ASSERT_TRUE(event.egress);
		EXPECT_EQ(event.egress->egress_id, "EG_1");
		ASSERT_TRUE(event.ingress);
		EXPECT_EQ(event.ingress->ingress_id, "IN_1");
	};
	callbacks.on_participant_joined = [&](const livekit::server::WebhookEvent& event) {
		++participant_events;
		ASSERT_TRUE(event.participant);
		EXPECT_EQ(event.participant->identity, "alice");
	};
	livekit::server::WebhookReceiver receiver("key", "secret", std::move(callbacks));
	receiver.ReceiveAndDispatch(body, "Bearer " + WebhookToken(body, now));
	EXPECT_EQ(all_events, 1);
	EXPECT_EQ(participant_events, 1);
}

TEST(WebhookReceiverTest, RejectsTamperingAndUnknownKeys) {
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

TEST(WebhookReceiverTest, RejectsInvalidTokenTimes) {
	const std::string body = R"({"event":"room_finished","id":"EV_3"})";
	const auto now = std::chrono::system_clock::now();
	livekit::server::WebhookReceiver receiver("key", "secret");
	ExpectAuthenticationFailure(receiver, body,
	                            WebhookToken(body, now, "key", "secret", std::chrono::minutes(-2)));
	ExpectAuthenticationFailure(
	    receiver, body,
	    WebhookToken(body, now, "key", "secret", std::chrono::minutes(5), std::chrono::minutes(2)));
}

TEST(WebhookReceiverTest, ReplacesCallbacksAndAcceptsUnknownEvents) {
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
	EXPECT_EQ(first, 1);
	EXPECT_EQ(second, 1);
}

} // namespace
