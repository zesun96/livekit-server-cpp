#pragma once

#include "livekit/server/model/models.h"

#if defined(LIVEKIT_SERVER_BUILDING_LIBRARY) || defined(LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER_API)
#include "livekit/server/protocol_fwd.h"
#endif

#include <memory>

namespace livekit::server {
namespace detail {
class ClientContext;
}

class EgressClient {
public:
	explicit EgressClient(std::shared_ptr<detail::ClientContext> context);

	[[nodiscard]] model::EgressInfo StartEgress(const model::StartEgressRequest& request) const;
	[[nodiscard]] model::EgressInfo
	StartRoomCompositeEgress(const model::RoomCompositeEgressRequest& request) const;
	[[nodiscard]] model::EgressInfo StartWebEgress(const model::WebEgressRequest& request) const;
	[[nodiscard]] model::EgressInfo
	StartParticipantEgress(const model::ParticipantEgressRequest& request) const;
	[[nodiscard]] model::EgressInfo
	StartTrackCompositeEgress(const model::TrackCompositeEgressRequest& request) const;
	[[nodiscard]] model::EgressInfo
	StartTrackEgress(const model::TrackEgressRequest& request) const;
	[[nodiscard]] model::EgressInfo UpdateLayout(const model::UpdateLayoutRequest& request) const;
	[[nodiscard]] model::EgressInfo UpdateStream(const model::UpdateStreamRequest& request) const;
	[[nodiscard]] model::ListEgressResponse
	ListEgress(const model::ListEgressRequest& request) const;
	[[nodiscard]] model::EgressInfo StopEgress(const model::StopEgressRequest& request) const;

#if defined(LIVEKIT_SERVER_BUILDING_LIBRARY) || defined(LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER_API)
	// Protobuf compatibility overloads are enabled by LiveKitServer::protobuf_adapter.
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
#endif

private:
	std::shared_ptr<detail::ClientContext> context_;
};

} // namespace livekit::server
