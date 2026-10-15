package io.livekit.server;

import static io.livekit.server.Models.*;

public final class RoomServiceClient {
    private final LiveKitApi api;
    RoomServiceClient(LiveKitApi api) { this.api = api; }
    public Room createRoom(CreateRoomRequest request) {
        return new Room(api.call("Room.CreateRoom", request));
    }
    public ListRoomsResponse listRooms(ListRoomsRequest request) {
        return new ListRoomsResponse(api.call("Room.ListRooms", request));
    }
    public DeleteRoomResponse deleteRoom(DeleteRoomRequest request) {
        return new DeleteRoomResponse(api.call("Room.DeleteRoom", request));
    }
    public ListParticipantsResponse listParticipants(ListParticipantsRequest request) {
        return new ListParticipantsResponse(api.call("Room.ListParticipants", request));
    }
    public ParticipantInfo getParticipant(RoomParticipantIdentity request) {
        return new ParticipantInfo(api.call("Room.GetParticipant", request));
    }
    public RemoveParticipantResponse removeParticipant(RoomParticipantIdentity request) {
        return new RemoveParticipantResponse(api.call("Room.RemoveParticipant", request));
    }
    public MuteRoomTrackResponse mutePublishedTrack(MuteRoomTrackRequest request) {
        return new MuteRoomTrackResponse(api.call("Room.MutePublishedTrack", request));
    }
    public ParticipantInfo updateParticipant(UpdateParticipantRequest request) {
        return new ParticipantInfo(api.call("Room.UpdateParticipant", request));
    }
    public UpdateSubscriptionsResponse updateSubscriptions(UpdateSubscriptionsRequest request) {
        return new UpdateSubscriptionsResponse(api.call("Room.UpdateSubscriptions", request));
    }
    public SendDataResponse sendData(SendDataRequest request) {
        return new SendDataResponse(api.call("Room.SendData", request));
    }
    public Room updateRoomMetadata(UpdateRoomMetadataRequest request) {
        return new Room(api.call("Room.UpdateRoomMetadata", request));
    }
    public ForwardParticipantResponse forwardParticipant(ForwardParticipantRequest request) {
        return new ForwardParticipantResponse(api.call("Room.ForwardParticipant", request));
    }
    public MoveParticipantResponse moveParticipant(MoveParticipantRequest request) {
        return new MoveParticipantResponse(api.call("Room.MoveParticipant", request));
    }
    public PerformRpcResponse performRpc(PerformRpcRequest request) {
        return new PerformRpcResponse(api.call("Room.PerformRpc", request));
    }
    public ListRoomsResponse listRooms() { return listRooms(new ListRoomsRequest()); }
}
