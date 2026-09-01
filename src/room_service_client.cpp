#include "livekit/server/room_service_client.h"

#include "detail/client_context.h"
#include "livekit_room.pb.h"

#ifdef _WIN32
#include <windows.h>

#include <bcrypt.h>
#endif

#include <array>
#include <random>
#include <utility>

namespace livekit::server {
namespace {

template <typename Response, typename Request>
Response Call(const std::shared_ptr<detail::ClientContext>& context, const char* method,
              const Request& request, detail::RequestGrant grant) {
	Response response;
	context->Call("RoomService", method, request, &response, grant);
	return response;
}

detail::RequestGrant RoomGrant(bool create = false, bool list = false, std::string room = {},
                               std::string destination = {}) {
	VideoGrant video;
	video.room_create = create;
	video.room_list = list;
	video.room_admin = !room.empty();
	video.room = std::move(room);
	video.destination_room = std::move(destination);
	return {.video = std::move(video)};
}

std::string Nonce() {
	std::array<char, 16> bytes{};
#ifdef _WIN32
	if (BCRYPT_SUCCESS(BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(bytes.data()),
	                                   static_cast<ULONG>(bytes.size()),
	                                   BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
		return {bytes.data(), bytes.size()};
	}
#else
	std::random_device random;
	for (auto& byte : bytes) {
		byte = static_cast<char>(random());
	}
	return {bytes.data(), bytes.size()};
#endif
	return {};
}

} // namespace

RoomServiceClient::RoomServiceClient(std::shared_ptr<detail::ClientContext> context)
    : context_(std::move(context)) {}

livekit::ListRoomsResponse RoomServiceClient::ListRooms() const {
	return ListRooms(livekit::ListRoomsRequest{});
}

livekit::Room RoomServiceClient::CreateRoom(const livekit::CreateRoomRequest& request) const {
	return Call<livekit::Room>(context_, "CreateRoom", request, RoomGrant(true));
}

livekit::ListRoomsResponse
RoomServiceClient::ListRooms(const livekit::ListRoomsRequest& request) const {
	return Call<livekit::ListRoomsResponse>(context_, "ListRooms", request, RoomGrant(false, true));
}

livekit::DeleteRoomResponse
RoomServiceClient::DeleteRoom(const livekit::DeleteRoomRequest& request) const {
	return Call<livekit::DeleteRoomResponse>(context_, "DeleteRoom", request, RoomGrant(true));
}

livekit::ListParticipantsResponse
RoomServiceClient::ListParticipants(const livekit::ListParticipantsRequest& request) const {
	return Call<livekit::ListParticipantsResponse>(context_, "ListParticipants", request,
	                                               RoomGrant(false, false, request.room()));
}

livekit::ParticipantInfo
RoomServiceClient::GetParticipant(const livekit::RoomParticipantIdentity& request) const {
	return Call<livekit::ParticipantInfo>(context_, "GetParticipant", request,
	                                      RoomGrant(false, false, request.room()));
}

livekit::RemoveParticipantResponse
RoomServiceClient::RemoveParticipant(const livekit::RoomParticipantIdentity& request) const {
	return Call<livekit::RemoveParticipantResponse>(context_, "RemoveParticipant", request,
	                                                RoomGrant(false, false, request.room()));
}

livekit::MuteRoomTrackResponse
RoomServiceClient::MutePublishedTrack(const livekit::MuteRoomTrackRequest& request) const {
	return Call<livekit::MuteRoomTrackResponse>(context_, "MutePublishedTrack", request,
	                                            RoomGrant(false, false, request.room()));
}

livekit::ParticipantInfo
RoomServiceClient::UpdateParticipant(const livekit::UpdateParticipantRequest& request) const {
	return Call<livekit::ParticipantInfo>(context_, "UpdateParticipant", request,
	                                      RoomGrant(false, false, request.room()));
}

livekit::UpdateSubscriptionsResponse
RoomServiceClient::UpdateSubscriptions(const livekit::UpdateSubscriptionsRequest& request) const {
	return Call<livekit::UpdateSubscriptionsResponse>(context_, "UpdateSubscriptions", request,
	                                                  RoomGrant(false, false, request.room()));
}

livekit::SendDataResponse
RoomServiceClient::SendData(const livekit::SendDataRequest& request) const {
	auto request_with_nonce = request;
	if (request_with_nonce.nonce().empty()) {
		request_with_nonce.set_nonce(Nonce());
	}
	return Call<livekit::SendDataResponse>(context_, "SendData", request_with_nonce,
	                                       RoomGrant(false, false, request.room()));
}

livekit::Room
RoomServiceClient::UpdateRoomMetadata(const livekit::UpdateRoomMetadataRequest& request) const {
	return Call<livekit::Room>(context_, "UpdateRoomMetadata", request,
	                           RoomGrant(false, false, request.room()));
}

livekit::ForwardParticipantResponse
RoomServiceClient::ForwardParticipant(const livekit::ForwardParticipantRequest& request) const {
	return Call<livekit::ForwardParticipantResponse>(
	    context_, "ForwardParticipant", request,
	    RoomGrant(false, false, request.room(), request.destination_room()));
}

livekit::MoveParticipantResponse
RoomServiceClient::MoveParticipant(const livekit::MoveParticipantRequest& request) const {
	return Call<livekit::MoveParticipantResponse>(
	    context_, "MoveParticipant", request,
	    RoomGrant(false, false, request.room(), request.destination_room()));
}

livekit::PerformRpcResponse
RoomServiceClient::PerformRpc(const livekit::PerformRpcRequest& request) const {
	return Call<livekit::PerformRpcResponse>(context_, "PerformRpc", request,
	                                         RoomGrant(false, false, request.room()));
}

} // namespace livekit::server
