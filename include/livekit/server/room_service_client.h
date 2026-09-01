#pragma once

#include "livekit_room.pb.h"

#include <memory>

namespace livekit::server {
namespace detail {
class ClientContext;
}

class RoomServiceClient {
public:
	explicit RoomServiceClient(std::shared_ptr<detail::ClientContext> context);

	[[nodiscard]] livekit::Room CreateRoom(const livekit::CreateRoomRequest& request) const;
	[[nodiscard]] livekit::ListRoomsResponse
	ListRooms(const livekit::ListRoomsRequest& request = {}) const;
	[[nodiscard]] livekit::DeleteRoomResponse
	DeleteRoom(const livekit::DeleteRoomRequest& request) const;
	[[nodiscard]] livekit::ListParticipantsResponse
	ListParticipants(const livekit::ListParticipantsRequest& request) const;
	[[nodiscard]] livekit::ParticipantInfo
	GetParticipant(const livekit::RoomParticipantIdentity& request) const;
	[[nodiscard]] livekit::RemoveParticipantResponse
	RemoveParticipant(const livekit::RoomParticipantIdentity& request) const;
	[[nodiscard]] livekit::MuteRoomTrackResponse
	MutePublishedTrack(const livekit::MuteRoomTrackRequest& request) const;
	[[nodiscard]] livekit::ParticipantInfo
	UpdateParticipant(const livekit::UpdateParticipantRequest& request) const;
	[[nodiscard]] livekit::UpdateSubscriptionsResponse
	UpdateSubscriptions(const livekit::UpdateSubscriptionsRequest& request) const;
	[[nodiscard]] livekit::SendDataResponse SendData(const livekit::SendDataRequest& request) const;
	[[nodiscard]] livekit::Room
	UpdateRoomMetadata(const livekit::UpdateRoomMetadataRequest& request) const;
	[[nodiscard]] livekit::ForwardParticipantResponse
	ForwardParticipant(const livekit::ForwardParticipantRequest& request) const;
	[[nodiscard]] livekit::MoveParticipantResponse
	MoveParticipant(const livekit::MoveParticipantRequest& request) const;
	[[nodiscard]] livekit::PerformRpcResponse
	PerformRpc(const livekit::PerformRpcRequest& request) const;

private:
	std::shared_ptr<detail::ClientContext> context_;
};

} // namespace livekit::server
