#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
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

struct WebhookRoom {
	std::string sid;
	std::string name;
	std::string metadata;
};

struct WebhookParticipant {
	std::string sid;
	std::string identity;
	std::string name;
	std::string metadata;
};

struct WebhookTrack {
	std::string sid;
	std::string name;
};

struct WebhookEgress {
	std::string egress_id;
	std::string room_id;
	std::string room_name;
};

struct WebhookIngress {
	std::string ingress_id;
	std::string name;
	std::string room_name;
};

// Public, protobuf-independent representation of a verified webhook. raw_body
// contains the complete event JSON, including fields added by newer servers.
struct WebhookEvent {
	std::string event;
	std::string id;
	std::int64_t created_at{};
	std::optional<WebhookRoom> room;
	std::optional<WebhookParticipant> participant;
	std::optional<WebhookTrack> track;
	std::optional<WebhookEgress> egress;
	std::optional<WebhookIngress> ingress;
	std::string raw_body;
};

using WebhookCallback = std::function<void(const WebhookEvent&)>;
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

	[[nodiscard]] WebhookEvent Receive(std::string_view body, std::string_view authorization) const;
	void Dispatch(const WebhookEvent& event) const;
	void ReceiveAndDispatch(std::string_view body, std::string_view authorization) const;

private:
	[[nodiscard]] WebhookEvent ReceiveAt(std::string_view body, std::string_view authorization,
	                                     std::chrono::system_clock::time_point now) const;

	WebhookKeyProvider key_provider_;
	mutable std::mutex callbacks_mutex_;
	WebhookCallbacks callbacks_;
};

} // namespace livekit::server
