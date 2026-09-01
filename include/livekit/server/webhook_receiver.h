#pragma once

#include "livekit_webhook.pb.h"

#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

namespace livekit::server {

namespace webhook_event {
inline constexpr std::string_view room_started = "room_started";
inline constexpr std::string_view room_finished = "room_finished";
inline constexpr std::string_view participant_joined = "participant_joined";
inline constexpr std::string_view participant_left = "participant_left";
inline constexpr std::string_view participant_connection_aborted = "participant_connection_aborted";
inline constexpr std::string_view track_published = "track_published";
inline constexpr std::string_view track_unpublished = "track_unpublished";
inline constexpr std::string_view egress_started = "egress_started";
inline constexpr std::string_view egress_updated = "egress_updated";
inline constexpr std::string_view egress_ended = "egress_ended";
inline constexpr std::string_view ingress_started = "ingress_started";
inline constexpr std::string_view ingress_ended = "ingress_ended";
} // namespace webhook_event

using WebhookCallback = std::function<void(const livekit::WebhookEvent&)>;
using WebhookKeyProvider = std::function<std::string(const std::string& api_key)>;

struct WebhookCallbacks {
	// Invoked for every verified event, before its event-specific callback.
	WebhookCallback on_event;
	WebhookCallback on_room_started;
	WebhookCallback on_room_finished;
	WebhookCallback on_participant_joined;
	WebhookCallback on_participant_left;
	WebhookCallback on_participant_connection_aborted;
	WebhookCallback on_track_published;
	WebhookCallback on_track_unpublished;
	WebhookCallback on_egress_started;
	WebhookCallback on_egress_updated;
	WebhookCallback on_egress_ended;
	WebhookCallback on_ingress_started;
	WebhookCallback on_ingress_ended;
};

// Verifies and dispatches LiveKit webhook events. It does not own an HTTP
// server: pass the raw request body and Authorization header from the
// application's HTTP framework. Callbacks execute synchronously on the
// calling thread and may be replaced safely while events are being handled.
class WebhookReceiver {
public:
	WebhookReceiver(std::string api_key, std::string api_secret, WebhookCallbacks callbacks = {});
	WebhookReceiver(std::map<std::string, std::string> api_secrets,
	                WebhookCallbacks callbacks = {});
	WebhookReceiver(WebhookKeyProvider key_provider, WebhookCallbacks callbacks = {});

	void SetCallbacks(WebhookCallbacks callbacks);

	[[nodiscard]] livekit::WebhookEvent Receive(std::string_view body,
	                                            std::string_view authorization) const;
	void Dispatch(const livekit::WebhookEvent& event) const;
	void ReceiveAndDispatch(std::string_view body, std::string_view authorization) const;

private:
	[[nodiscard]] livekit::WebhookEvent ReceiveAt(std::string_view body,
	                                              std::string_view authorization,
	                                              std::chrono::system_clock::time_point now) const;

	WebhookKeyProvider key_provider_;
	mutable std::mutex callbacks_mutex_;
	WebhookCallbacks callbacks_;
};

} // namespace livekit::server
