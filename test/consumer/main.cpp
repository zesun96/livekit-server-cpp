#include <livekit/server/access_token.h>
#include <livekit/server/livekit_api.h>

#include <chrono>
#include <string>
#include <utility>

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
	(void)receiver;
	return token.empty() ? 1 : 0;
}
