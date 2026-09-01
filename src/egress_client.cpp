#include "livekit/server/egress_client.h"

#include "detail/client_context.h"

#include <utility>

namespace livekit::server {
namespace {

template <typename Response, typename Request>
Response Call(const std::shared_ptr<detail::ClientContext>& context, const char* method,
              const Request& request) {
	Response response;
	detail::RequestGrant grant{.video = VideoGrant{.room_record = true}};
	context->Call("Egress", method, request, &response, grant);
	return response;
}

} // namespace

EgressClient::EgressClient(std::shared_ptr<detail::ClientContext> context)
    : context_(std::move(context)) {}

livekit::EgressInfo EgressClient::StartEgress(const livekit::StartEgressRequest& request) const {
	return Call<livekit::EgressInfo>(context_, "StartEgress", request);
}

livekit::EgressInfo
EgressClient::StartRoomCompositeEgress(const livekit::RoomCompositeEgressRequest& request) const {
	return Call<livekit::EgressInfo>(context_, "StartRoomCompositeEgress", request);
}

livekit::EgressInfo EgressClient::StartWebEgress(const livekit::WebEgressRequest& request) const {
	return Call<livekit::EgressInfo>(context_, "StartWebEgress", request);
}

livekit::EgressInfo
EgressClient::StartParticipantEgress(const livekit::ParticipantEgressRequest& request) const {
	return Call<livekit::EgressInfo>(context_, "StartParticipantEgress", request);
}

livekit::EgressInfo
EgressClient::StartTrackCompositeEgress(const livekit::TrackCompositeEgressRequest& request) const {
	return Call<livekit::EgressInfo>(context_, "StartTrackCompositeEgress", request);
}

livekit::EgressInfo
EgressClient::StartTrackEgress(const livekit::TrackEgressRequest& request) const {
	return Call<livekit::EgressInfo>(context_, "StartTrackEgress", request);
}

livekit::EgressInfo EgressClient::UpdateLayout(const livekit::UpdateLayoutRequest& request) const {
	return Call<livekit::EgressInfo>(context_, "UpdateLayout", request);
}

livekit::EgressInfo EgressClient::UpdateStream(const livekit::UpdateStreamRequest& request) const {
	return Call<livekit::EgressInfo>(context_, "UpdateStream", request);
}

livekit::ListEgressResponse
EgressClient::ListEgress(const livekit::ListEgressRequest& request) const {
	return Call<livekit::ListEgressResponse>(context_, "ListEgress", request);
}

livekit::EgressInfo EgressClient::StopEgress(const livekit::StopEgressRequest& request) const {
	return Call<livekit::EgressInfo>(context_, "StopEgress", request);
}

} // namespace livekit::server
