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

class RoomServiceClient {
public:
	explicit RoomServiceClient(std::shared_ptr<detail::ClientContext> context);

	[[nodiscard]] model::Room CreateRoom(const model::CreateRoomRequest& request) const;
	[[nodiscard]] model::ListRoomsResponse ListRoomsModel() const;
	[[nodiscard]] model::ListRoomsResponse ListRooms(const model::ListRoomsRequest& request) const;
	[[nodiscard]] model::DeleteRoomResponse
	DeleteRoom(const model::DeleteRoomRequest& request) const;
	[[nodiscard]] model::ListParticipantsResponse
	ListParticipants(const model::ListParticipantsRequest& request) const;
	[[nodiscard]] model::ParticipantInfo
	GetParticipant(const model::RoomParticipantIdentity& request) const;
	[[nodiscard]] model::RemoveParticipantResponse
	RemoveParticipant(const model::RoomParticipantIdentity& request) const;
	[[nodiscard]] model::MuteRoomTrackResponse
	MutePublishedTrack(const model::MuteRoomTrackRequest& request) const;
	[[nodiscard]] model::ParticipantInfo
	UpdateParticipant(const model::UpdateParticipantRequest& request) const;
	[[nodiscard]] model::UpdateSubscriptionsResponse
	UpdateSubscriptions(const model::UpdateSubscriptionsRequest& request) const;
	[[nodiscard]] model::SendDataResponse SendData(const model::SendDataRequest& request) const;
	[[nodiscard]] model::Room
	UpdateRoomMetadata(const model::UpdateRoomMetadataRequest& request) const;
	[[nodiscard]] model::ForwardParticipantResponse
	ForwardParticipant(const model::ForwardParticipantRequest& request) const;
	[[nodiscard]] model::MoveParticipantResponse
	MoveParticipant(const model::MoveParticipantRequest& request) const;
	[[nodiscard]] model::PerformRpcResponse
	PerformRpc(const model::PerformRpcRequest& request) const;

#if defined(LIVEKIT_SERVER_BUILDING_LIBRARY) || defined(LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER_API)
	// Protobuf compatibility overloads are enabled by LiveKitServer::protobuf_adapter.
	[[nodiscard]] livekit::Room CreateRoom(const livekit::CreateRoomRequest& request) const;
	[[nodiscard]] livekit::ListRoomsResponse ListRooms() const;
	[[nodiscard]] livekit::ListRoomsResponse
	ListRooms(const livekit::ListRoomsRequest& request) const;
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
#endif

private:
	std::shared_ptr<detail::ClientContext> context_;
};

} // namespace livekit::server
