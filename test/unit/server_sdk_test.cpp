#include "livekit/server/access_token.h"
#include "livekit/server/error.h"
#include "livekit/server/livekit_api.h"
#include "livekit_agent_dispatch.pb.h"
#include "livekit_connector.pb.h"
#include "livekit_ingress.pb.h"
#include "livekit_room.pb.h"
#include "livekit_sip.pb.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void Require(bool condition, const char* message) {
	if (!condition) {
		throw std::runtime_error(message);
	}
}

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
		Require(position != std::string::npos, "invalid base64url token");
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
			Require(create_request.ParseFromString(request.body), "request protobuf did not parse");
			livekit::Room room;
			room.set_name(create_request.name());
			std::string body;
			Require(room.SerializeToString(&body), "response protobuf did not serialize");
			return {.status_code = 200, .body = std::move(body)};
		}
		livekit::ListRoomsResponse rooms;
		std::string body;
		Require(rooms.SerializeToString(&body), "response protobuf did not serialize");
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
	Require(authorization.starts_with("Bearer "), "bearer token missing");
	const auto token = authorization.substr(7);
	const auto first_dot = token.find('.');
	const auto second_dot = token.find('.', first_dot + 1);
	Require(first_dot != std::string::npos && second_dot != std::string::npos,
	        "bearer token is not a JWT");
	return Base64UrlDecode(token.substr(first_dot + 1, second_dot - first_dot - 1));
}

void TestAccessToken() {
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
	Require(first_dot != std::string::npos && second_dot != std::string::npos,
	        "JWT must have three segments");
	Require(Base64UrlDecode(token.substr(0, first_dot)) == R"({"alg":"HS256","typ":"JWT"})",
	        "JWT header mismatch");
	const auto payload = Base64UrlDecode(token.substr(first_dot + 1, second_dot - first_dot - 1));
	Require(payload.find(R"("iss":"key")") != std::string::npos, "JWT issuer missing");
	Require(payload.find(R"("sub":"alice")") != std::string::npos, "JWT subject missing");
	Require(payload.find(R"("exp":1700003600)") != std::string::npos, "JWT expiration mismatch");
	Require(payload.find(R"("roomJoin":true)") != std::string::npos, "roomJoin grant missing");
	Require(payload.find(R"("canPublish":false)") != std::string::npos,
	        "explicit false grant missing");
	Require(!token.substr(second_dot + 1).empty(), "JWT signature missing");
}

void TestRoomRequest() {
	auto transport = std::make_shared<RecordingTransport>();
	livekit::server::ApiOptions options;
	options.url = "ws://localhost:7880/";
	options.access_token = "fixed-token";
	options.transport = transport;
	livekit::server::LiveKitApi api(std::move(options));

	livekit::CreateRoomRequest request;
	request.set_name("sdk-test");
	const auto room = api.Room().CreateRoom(request);
	Require(room.name() == "sdk-test", "CreateRoom response mismatch");
	Require(transport->last_request.url ==
	            "http://localhost:7880/twirp/livekit.RoomService/CreateRoom",
	        "Twirp URL mismatch");
	Require(Header(transport->last_request, "Content-Type") == "application/protobuf",
	        "protobuf content type missing");
	Require(Header(transport->last_request, "Authorization") == "Bearer fixed-token",
	        "authorization header mismatch");
	Require(!Header(transport->last_request, "X-Livekit-Request-Id").empty(), "request id missing");
}

void TestTwirpError() {
	auto transport = std::make_shared<RecordingTransport>();
	transport->fail = true;
	livekit::server::ApiOptions options;
	options.url = "http://localhost:7880";
	options.access_token = "fixed-token";
	options.transport = transport;
	livekit::server::LiveKitApi api(std::move(options));
	try {
		(void)api.Room().ListRooms();
		throw std::runtime_error("expected Twirp error");
	} catch (const livekit::server::Error& error) {
		Require(error.code() == livekit::server::ErrorCode::http, "wrong error category");
		Require(error.http_status() == 403, "wrong HTTP status");
		Require(error.twirp_code() == "permission_denied", "wrong Twirp code");
		Require(std::string(error.what()) == "not allowed", "wrong Twirp message");
	}
}

void TestServiceRoutesAndGrants() {
	auto transport = std::make_shared<RecordingTransport>();
	livekit::server::ApiOptions options;
	options.url = "https://livekit.example";
	options.api_key = "key";
	options.api_secret = "secret";
	options.transport = transport;
	livekit::server::LiveKitApi api(std::move(options));

	(void)api.Egress().ListEgress({});
	Require(transport->last_request.url.ends_with("/twirp/livekit.Egress/ListEgress"),
	        "Egress route mismatch");
	Require(BearerPayload(transport->last_request).find(R"("roomRecord":true)") !=
	            std::string::npos,
	        "Egress grant mismatch");

	(void)api.Ingress().ListIngress({});
	Require(transport->last_request.url.ends_with("/twirp/livekit.Ingress/ListIngress"),
	        "Ingress route mismatch");
	Require(BearerPayload(transport->last_request).find(R"("ingressAdmin":true)") !=
	            std::string::npos,
	        "Ingress grant mismatch");

	(void)api.SIP().ListTrunks({});
	Require(transport->last_request.url.ends_with("/twirp/livekit.SIP/ListSIPTrunk"),
	        "SIP route mismatch");
	Require(BearerPayload(transport->last_request).find(R"("sip":{"admin":true})") !=
	            std::string::npos,
	        "SIP grant mismatch");

	livekit::ListAgentDispatchRequest dispatch;
	dispatch.set_room("agent-room");
	(void)api.AgentDispatch().ListDispatch(dispatch);
	Require(
	    transport->last_request.url.ends_with("/twirp/livekit.AgentDispatchService/ListDispatch"),
	    "Agent dispatch route mismatch");
	const auto dispatch_payload = BearerPayload(transport->last_request);
	Require(dispatch_payload.find(R"("roomAdmin":true)") != std::string::npos &&
	            dispatch_payload.find(R"("room":"agent-room")") != std::string::npos,
	        "Agent dispatch grant mismatch");

	(void)api.Connector().DisconnectWhatsAppCall({});
	Require(
	    transport->last_request.url.ends_with("/twirp/livekit.Connector/DisconnectWhatsAppCall"),
	    "Connector route mismatch");
	Require(BearerPayload(transport->last_request).find(R"("roomCreate":true)") !=
	            std::string::npos,
	        "Connector grant mismatch");
}

void TestConfigurationValidation() {
	livekit::server::ApiOptions options;
	options.url = "http://localhost:7880";
	options.api_key = "key-without-secret";
	try {
		livekit::server::LiveKitApi api(std::move(options));
		throw std::runtime_error("expected authentication error");
	} catch (const livekit::server::Error& error) {
		Require(error.code() == livekit::server::ErrorCode::authentication,
		        "wrong configuration error category");
	}
}

} // namespace

int main() {
	try {
		TestAccessToken();
		TestRoomRequest();
		TestTwirpError();
		TestServiceRoutesAndGrants();
		TestConfigurationValidation();
		std::cout << "all unit tests passed\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "test failure: " << error.what() << '\n';
		return 1;
	}
}
