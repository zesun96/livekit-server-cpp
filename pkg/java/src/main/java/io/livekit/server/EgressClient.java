package io.livekit.server;

import static io.livekit.server.Models.*;

public final class EgressClient {
    private final LiveKitApi api;
    EgressClient(LiveKitApi api) { this.api = api; }
    public EgressInfo startEgress(StartEgressRequest request) {
        return new EgressInfo(api.call("Egress.StartEgress", request));
    }
    public EgressInfo startRoomCompositeEgress(RoomCompositeEgressRequest request) {
        return new EgressInfo(api.call("Egress.StartRoomCompositeEgress", request));
    }
    public EgressInfo startWebEgress(WebEgressRequest request) {
        return new EgressInfo(api.call("Egress.StartWebEgress", request));
    }
    public EgressInfo startParticipantEgress(ParticipantEgressRequest request) {
        return new EgressInfo(api.call("Egress.StartParticipantEgress", request));
    }
    public EgressInfo startTrackCompositeEgress(TrackCompositeEgressRequest request) {
        return new EgressInfo(api.call("Egress.StartTrackCompositeEgress", request));
    }
    public EgressInfo startTrackEgress(TrackEgressRequest request) {
        return new EgressInfo(api.call("Egress.StartTrackEgress", request));
    }
    public EgressInfo updateLayout(UpdateLayoutRequest request) {
        return new EgressInfo(api.call("Egress.UpdateLayout", request));
    }
    public EgressInfo updateStream(UpdateStreamRequest request) {
        return new EgressInfo(api.call("Egress.UpdateStream", request));
    }
    public ListEgressResponse listEgress(ListEgressRequest request) {
        return new ListEgressResponse(api.call("Egress.ListEgress", request));
    }
    public EgressInfo stopEgress(StopEgressRequest request) {
        return new EgressInfo(api.call("Egress.StopEgress", request));
    }
}
