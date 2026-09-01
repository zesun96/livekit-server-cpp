#pragma once

#include "livekit_egress.pb.h"

#include <memory>

namespace livekit::server {
namespace detail {
class ClientContext;
}

class EgressClient {
public:
	explicit EgressClient(std::shared_ptr<detail::ClientContext> context);

	[[nodiscard]] livekit::EgressInfo StartEgress(const livekit::StartEgressRequest& request) const;
	[[nodiscard]] livekit::EgressInfo
	StartRoomCompositeEgress(const livekit::RoomCompositeEgressRequest& request) const;
	[[nodiscard]] livekit::EgressInfo
	StartWebEgress(const livekit::WebEgressRequest& request) const;
	[[nodiscard]] livekit::EgressInfo
	StartParticipantEgress(const livekit::ParticipantEgressRequest& request) const;
	[[nodiscard]] livekit::EgressInfo
	StartTrackCompositeEgress(const livekit::TrackCompositeEgressRequest& request) const;
	[[nodiscard]] livekit::EgressInfo
	StartTrackEgress(const livekit::TrackEgressRequest& request) const;
	[[nodiscard]] livekit::EgressInfo
	UpdateLayout(const livekit::UpdateLayoutRequest& request) const;
	[[nodiscard]] livekit::EgressInfo
	UpdateStream(const livekit::UpdateStreamRequest& request) const;
	[[nodiscard]] livekit::ListEgressResponse
	ListEgress(const livekit::ListEgressRequest& request) const;
	[[nodiscard]] livekit::EgressInfo StopEgress(const livekit::StopEgressRequest& request) const;

private:
	std::shared_ptr<detail::ClientContext> context_;
};

} // namespace livekit::server
