#include "livekit/server/access_token.h"
#include "livekit/server/error.h"
#include "livekit/server/livekit_api.h"
#include "livekit_agent_dispatch.pb.h"
#include "livekit_connector.pb.h"
#include "livekit_ingress.pb.h"
#include "livekit_room.pb.h"
#include "livekit_sip.pb.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <utility>

namespace {

std::string Base64UrlDecode(std::string value) {
	for (auto& ch : value) {
		if (ch == '-') {
			ch = '+';
		} else if (ch == '_') {
			ch = '/';
		}
	}
	while (value.size() % 4 != 0) {
		value.push_back('=');
	}
	static const std::string alphabet =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string output;
	unsigned int buffer = 0;
	int bits = 0;
	for (const char ch : value) {
		if (ch == '=') {
			break;
		}
		const auto position = alphabet.find(ch);
		if (position == std::string::npos) {
			return {};
		}
		buffer = (buffer << 6) | static_cast<unsigned int>(position);
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			output.push_back(static_cast<char>((buffer >> bits) & 0xff));
		}
	}
	return output;
}

class RecordingTransport final : public livekit::server::HttpTransport {
public:
	livekit::server::HttpResponse Send(const livekit::server::HttpRequest& request) override {
		last_request = request;
		if (fail) {
			return {.status_code = 403,
			        .body = R"({"code":"permission_denied","msg":"not allowed"})"};
		}
		livekit::CreateRoomRequest create_request;
		if (request.url.ends_with("/CreateRoom")) {
			EXPECT_TRUE(create_request.ParseFromString(request.body));
			livekit::Room room;
			room.set_name(create_request.name());
			std::string body;
			EXPECT_TRUE(room.SerializeToString(&body));
			return {.status_code = 200, .body = std::move(body)};
		}
		livekit::ListRoomsResponse rooms;
		std::string body;
		EXPECT_TRUE(rooms.SerializeToString(&body));
		return {.status_code = 200, .body = std::move(body)};
	}

	livekit::server::HttpRequest last_request;
	bool fail{};
};

std::string Header(const livekit::server::HttpRequest& request, const std::string& name) {
	for (const auto& [header_name, value] : request.headers) {
		if (header_name == name) {
			return value;
		}
	}
	return {};
}

std::string BearerPayload(const livekit::server::HttpRequest& request) {
	const auto authorization = Header(request, "Authorization");
	EXPECT_TRUE(authorization.starts_with("Bearer "));
	if (!authorization.starts_with("Bearer ")) {
		return {};
	}
	const auto token = authorization.substr(7);
	const auto first_dot = token.find('.');
	const auto second_dot = token.find('.', first_dot + 1);
	EXPECT_NE(first_dot, std::string::npos);
	EXPECT_NE(second_dot, std::string::npos);
	if (first_dot == std::string::npos || second_dot == std::string::npos) {
		return {};
	}
	return Base64UrlDecode(token.substr(first_dot + 1, second_dot - first_dot - 1));
}

TEST(AccessTokenTest, BuildsSignedJwtWithExplicitGrantValues) {
	livekit::server::VideoGrant grant;
	grant.room_join = true;
	grant.room = "room-a";
	grant.can_publish = false;
	const auto now = std::chrono::system_clock::time_point(std::chrono::seconds(1700000000));
	const auto token = livekit::server::AccessToken("key", "secret")
	                       .SetIdentity("alice")
	                       .SetName("Alice")
	                       .SetValidFor(std::chrono::hours(1))
	                       .SetVideoGrant(grant)
	                       .ToJwt(now);
	const auto first_dot = token.find('.');
	const auto second_dot = token.find('.', first_dot + 1);
	ASSERT_NE(first_dot, std::string::npos);
	ASSERT_NE(second_dot, std::string::npos);
	EXPECT_EQ(Base64UrlDecode(token.substr(0, first_dot)), R"({"alg":"HS256","typ":"JWT"})");
	const auto payload = Base64UrlDecode(token.substr(first_dot + 1, second_dot - first_dot - 1));
	EXPECT_NE(payload.find(R"("iss":"key")"), std::string::npos);
	EXPECT_NE(payload.find(R"("sub":"alice")"), std::string::npos);
	EXPECT_NE(payload.find(R"("exp":1700003600)"), std::string::npos);
	EXPECT_NE(payload.find(R"("roomJoin":true)"), std::string::npos);
	EXPECT_NE(payload.find(R"("canPublish":false)"), std::string::npos);
	EXPECT_FALSE(token.substr(second_dot + 1).empty());
}

TEST(LiveKitApiTest, SendsRoomRequestToExpectedTwirpRoute) {
	auto transport = std::make_shared<RecordingTransport>();
	livekit::server::ApiOptions options;
	options.url = "ws://localhost:7880/";
	options.access_token = "fixed-token";
	options.transport = transport;
	livekit::server::LiveKitApi api(std::move(options));

	livekit::CreateRoomRequest request;
	request.set_name("sdk-test");
	const auto room = api.Room().CreateRoom(request);
	EXPECT_EQ(room.name(), "sdk-test");
	EXPECT_EQ(transport->last_request.url,
	          "http://localhost:7880/twirp/livekit.RoomService/CreateRoom");
	EXPECT_EQ(Header(transport->last_request, "Content-Type"), "application/protobuf");
	EXPECT_EQ(Header(transport->last_request, "Authorization"), "Bearer fixed-token");
	EXPECT_FALSE(Header(transport->last_request, "X-Livekit-Request-Id").empty());
}

TEST(LiveKitApiTest, ConvertsTwirpFailureToSdkError) {
	auto transport = std::make_shared<RecordingTransport>();
	transport->fail = true;
	livekit::server::ApiOptions options;
	options.url = "http://localhost:7880";
	options.access_token = "fixed-token";
	options.transport = transport;
	livekit::server::LiveKitApi api(std::move(options));
	try {
		(void)api.Room().ListRooms();
		FAIL() << "expected Twirp error";
	} catch (const livekit::server::Error& error) {
		EXPECT_EQ(error.code(), livekit::server::ErrorCode::http);
		EXPECT_EQ(error.http_status(), 403);
		EXPECT_EQ(error.twirp_code(), "permission_denied");
		EXPECT_STREQ(error.what(), "not allowed");
	}
}

TEST(LiveKitApiTest, UsesExpectedServiceRoutesAndGrants) {
	auto transport = std::make_shared<RecordingTransport>();
	livekit::server::ApiOptions options;
	options.url = "https://livekit.example";
	options.api_key = "key";
	options.api_secret = "secret";
	options.transport = transport;
	livekit::server::LiveKitApi api(std::move(options));

	(void)api.Egress().ListEgress({});
	EXPECT_TRUE(transport->last_request.url.ends_with("/twirp/livekit.Egress/ListEgress"));
	EXPECT_NE(BearerPayload(transport->last_request).find(R"("roomRecord":true)"),
	          std::string::npos);

	(void)api.Ingress().ListIngress({});
	EXPECT_TRUE(transport->last_request.url.ends_with("/twirp/livekit.Ingress/ListIngress"));
	EXPECT_NE(BearerPayload(transport->last_request).find(R"("ingressAdmin":true)"),
	          std::string::npos);

	(void)api.SIP().ListTrunks({});
	EXPECT_TRUE(transport->last_request.url.ends_with("/twirp/livekit.SIP/ListSIPTrunk"));
	EXPECT_NE(BearerPayload(transport->last_request).find(R"("sip":{"admin":true})"),
	          std::string::npos);

	livekit::ListAgentDispatchRequest dispatch;
	dispatch.set_room("agent-room");
	(void)api.AgentDispatch().ListDispatch(dispatch);
	EXPECT_TRUE(
	    transport->last_request.url.ends_with("/twirp/livekit.AgentDispatchService/ListDispatch"));
	const auto dispatch_payload = BearerPayload(transport->last_request);
	EXPECT_NE(dispatch_payload.find(R"("roomAdmin":true)"), std::string::npos);
	EXPECT_NE(dispatch_payload.find(R"("room":"agent-room")"), std::string::npos);

	(void)api.Connector().DisconnectWhatsAppCall({});
	EXPECT_TRUE(
	    transport->last_request.url.ends_with("/twirp/livekit.Connector/DisconnectWhatsAppCall"));
	EXPECT_NE(BearerPayload(transport->last_request).find(R"("roomCreate":true)"),
	          std::string::npos);
}

TEST(LiveKitApiTest, RejectsIncompleteCredentials) {
	livekit::server::ApiOptions options;
	options.url = "http://localhost:7880";
	options.api_key = "key-without-secret";
	try {
		livekit::server::LiveKitApi api(std::move(options));
		FAIL() << "expected authentication error";
	} catch (const livekit::server::Error& error) {
		EXPECT_EQ(error.code(), livekit::server::ErrorCode::authentication);
	}
}

} // namespace
