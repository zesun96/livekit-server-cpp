#include <livekit/server/access_token.h>
#include <livekit/server/livekit_api.h>
#include <livekit/server/model/models.h>

#include <chrono>
#include <string>
#include <utility>

void CompileSdkModelServiceApi(livekit::server::LiveKitApi& api) {
	const auto request =
	    livekit::server::model::CreateRoomRequest::FromJson(R"({"name":"consumer-test"})");
	(void)api.Room().CreateRoom(request);
	(void)api.Room().ListRoomsModel();
}

int main() {
	livekit::server::VideoGrant grant;
	grant.room_join = true;
	grant.room = "consumer-test";
	const auto token = livekit::server::AccessToken("test-key", "test-secret")
	                       .SetIdentity("consumer")
	                       .SetValidFor(std::chrono::minutes(5))
	                       .SetVideoGrant(std::move(grant))
	                       .ToJwt();
	livekit::server::WebhookReceiver receiver("test-key", "test-secret");
	const auto request =
	    livekit::server::model::CreateRoomRequest::FromJson(R"({"name":"consumer-test"})");
	if (request.Json().empty()) {
		return 1;
	}
	(void)receiver;
	return token.empty() ? 1 : 0;
}
