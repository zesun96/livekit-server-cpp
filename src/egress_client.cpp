#include "livekit/server/egress_client.h"

#include "detail/client_context.h"
#include "detail/proto/json_codec.h"
#include "livekit_egress.pb.h"

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

#define LIVEKIT_EGRESS_MODEL_METHOD(response, method, request, proto_response, proto_request_type) \
	model::response EgressClient::method(const model::request& request) const {                    \
		auto protobuf_request = detail::proto::FromModel<livekit::proto_request_type>(request);    \
		return detail::proto::ToModel<model::response>(method(protobuf_request));                  \
	}

LIVEKIT_EGRESS_MODEL_METHOD(EgressInfo, StartEgress, StartEgressRequest, EgressInfo,
                            StartEgressRequest)
LIVEKIT_EGRESS_MODEL_METHOD(EgressInfo, StartRoomCompositeEgress, RoomCompositeEgressRequest,
                            EgressInfo, RoomCompositeEgressRequest)
LIVEKIT_EGRESS_MODEL_METHOD(EgressInfo, StartWebEgress, WebEgressRequest, EgressInfo,
                            WebEgressRequest)
LIVEKIT_EGRESS_MODEL_METHOD(EgressInfo, StartParticipantEgress, ParticipantEgressRequest,
                            EgressInfo, ParticipantEgressRequest)
LIVEKIT_EGRESS_MODEL_METHOD(EgressInfo, StartTrackCompositeEgress, TrackCompositeEgressRequest,
                            EgressInfo, TrackCompositeEgressRequest)
LIVEKIT_EGRESS_MODEL_METHOD(EgressInfo, StartTrackEgress, TrackEgressRequest, EgressInfo,
                            TrackEgressRequest)
LIVEKIT_EGRESS_MODEL_METHOD(EgressInfo, UpdateLayout, UpdateLayoutRequest, EgressInfo,
                            UpdateLayoutRequest)
LIVEKIT_EGRESS_MODEL_METHOD(EgressInfo, UpdateStream, UpdateStreamRequest, EgressInfo,
                            UpdateStreamRequest)
LIVEKIT_EGRESS_MODEL_METHOD(ListEgressResponse, ListEgress, ListEgressRequest, ListEgressResponse,
                            ListEgressRequest)
LIVEKIT_EGRESS_MODEL_METHOD(EgressInfo, StopEgress, StopEgressRequest, EgressInfo,
                            StopEgressRequest)

#undef LIVEKIT_EGRESS_MODEL_METHOD

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
