package io.livekit.server;

/** SDK-owned protobuf JSON models, matching the C++ public model API. */
public final class Models {
    private Models() {}
    public static final class Empty extends ProtocolModel {
        public Empty() { super("{}"); }
        public Empty(String json) { super(json); }
        public static Empty fromJson(String json) { return new Empty(json); }
    }
    public static final class CreateRoomRequest extends ProtocolModel {
        public CreateRoomRequest() { super("{}"); }
        public CreateRoomRequest(String json) { super(json); }
        public static CreateRoomRequest fromJson(String json) { return new CreateRoomRequest(json); }
    }
    public static final class Room extends ProtocolModel {
        public Room() { super("{}"); }
        public Room(String json) { super(json); }
        public static Room fromJson(String json) { return new Room(json); }
    }
    public static final class ListRoomsRequest extends ProtocolModel {
        public ListRoomsRequest() { super("{}"); }
        public ListRoomsRequest(String json) { super(json); }
        public static ListRoomsRequest fromJson(String json) { return new ListRoomsRequest(json); }
    }
    public static final class ListRoomsResponse extends ProtocolModel {
        public ListRoomsResponse() { super("{}"); }
        public ListRoomsResponse(String json) { super(json); }
        public static ListRoomsResponse fromJson(String json) { return new ListRoomsResponse(json); }
    }
    public static final class DeleteRoomRequest extends ProtocolModel {
        public DeleteRoomRequest() { super("{}"); }
        public DeleteRoomRequest(String json) { super(json); }
        public static DeleteRoomRequest fromJson(String json) { return new DeleteRoomRequest(json); }
    }
    public static final class DeleteRoomResponse extends ProtocolModel {
        public DeleteRoomResponse() { super("{}"); }
        public DeleteRoomResponse(String json) { super(json); }
        public static DeleteRoomResponse fromJson(String json) { return new DeleteRoomResponse(json); }
    }
    public static final class ListParticipantsRequest extends ProtocolModel {
        public ListParticipantsRequest() { super("{}"); }
        public ListParticipantsRequest(String json) { super(json); }
        public static ListParticipantsRequest fromJson(String json) { return new ListParticipantsRequest(json); }
    }
    public static final class ListParticipantsResponse extends ProtocolModel {
        public ListParticipantsResponse() { super("{}"); }
        public ListParticipantsResponse(String json) { super(json); }
        public static ListParticipantsResponse fromJson(String json) { return new ListParticipantsResponse(json); }
    }
    public static final class RoomParticipantIdentity extends ProtocolModel {
        public RoomParticipantIdentity() { super("{}"); }
        public RoomParticipantIdentity(String json) { super(json); }
        public static RoomParticipantIdentity fromJson(String json) { return new RoomParticipantIdentity(json); }
    }
    public static final class ParticipantInfo extends ProtocolModel {
        public ParticipantInfo() { super("{}"); }
        public ParticipantInfo(String json) { super(json); }
        public static ParticipantInfo fromJson(String json) { return new ParticipantInfo(json); }
    }
    public static final class RemoveParticipantResponse extends ProtocolModel {
        public RemoveParticipantResponse() { super("{}"); }
        public RemoveParticipantResponse(String json) { super(json); }
        public static RemoveParticipantResponse fromJson(String json) { return new RemoveParticipantResponse(json); }
    }
    public static final class MuteRoomTrackRequest extends ProtocolModel {
        public MuteRoomTrackRequest() { super("{}"); }
        public MuteRoomTrackRequest(String json) { super(json); }
        public static MuteRoomTrackRequest fromJson(String json) { return new MuteRoomTrackRequest(json); }
    }
    public static final class MuteRoomTrackResponse extends ProtocolModel {
        public MuteRoomTrackResponse() { super("{}"); }
        public MuteRoomTrackResponse(String json) { super(json); }
        public static MuteRoomTrackResponse fromJson(String json) { return new MuteRoomTrackResponse(json); }
    }
    public static final class UpdateParticipantRequest extends ProtocolModel {
        public UpdateParticipantRequest() { super("{}"); }
        public UpdateParticipantRequest(String json) { super(json); }
        public static UpdateParticipantRequest fromJson(String json) { return new UpdateParticipantRequest(json); }
    }
    public static final class UpdateSubscriptionsRequest extends ProtocolModel {
        public UpdateSubscriptionsRequest() { super("{}"); }
        public UpdateSubscriptionsRequest(String json) { super(json); }
        public static UpdateSubscriptionsRequest fromJson(String json) { return new UpdateSubscriptionsRequest(json); }
    }
    public static final class UpdateSubscriptionsResponse extends ProtocolModel {
        public UpdateSubscriptionsResponse() { super("{}"); }
        public UpdateSubscriptionsResponse(String json) { super(json); }
        public static UpdateSubscriptionsResponse fromJson(String json) { return new UpdateSubscriptionsResponse(json); }
    }
    public static final class SendDataRequest extends ProtocolModel {
        public SendDataRequest() { super("{}"); }
        public SendDataRequest(String json) { super(json); }
        public static SendDataRequest fromJson(String json) { return new SendDataRequest(json); }
    }
    public static final class SendDataResponse extends ProtocolModel {
        public SendDataResponse() { super("{}"); }
        public SendDataResponse(String json) { super(json); }
        public static SendDataResponse fromJson(String json) { return new SendDataResponse(json); }
    }
    public static final class UpdateRoomMetadataRequest extends ProtocolModel {
        public UpdateRoomMetadataRequest() { super("{}"); }
        public UpdateRoomMetadataRequest(String json) { super(json); }
        public static UpdateRoomMetadataRequest fromJson(String json) { return new UpdateRoomMetadataRequest(json); }
    }
    public static final class ForwardParticipantRequest extends ProtocolModel {
        public ForwardParticipantRequest() { super("{}"); }
        public ForwardParticipantRequest(String json) { super(json); }
        public static ForwardParticipantRequest fromJson(String json) { return new ForwardParticipantRequest(json); }
    }
    public static final class ForwardParticipantResponse extends ProtocolModel {
        public ForwardParticipantResponse() { super("{}"); }
        public ForwardParticipantResponse(String json) { super(json); }
        public static ForwardParticipantResponse fromJson(String json) { return new ForwardParticipantResponse(json); }
    }
    public static final class MoveParticipantRequest extends ProtocolModel {
        public MoveParticipantRequest() { super("{}"); }
        public MoveParticipantRequest(String json) { super(json); }
        public static MoveParticipantRequest fromJson(String json) { return new MoveParticipantRequest(json); }
    }
    public static final class MoveParticipantResponse extends ProtocolModel {
        public MoveParticipantResponse() { super("{}"); }
        public MoveParticipantResponse(String json) { super(json); }
        public static MoveParticipantResponse fromJson(String json) { return new MoveParticipantResponse(json); }
    }
    public static final class PerformRpcRequest extends ProtocolModel {
        public PerformRpcRequest() { super("{}"); }
        public PerformRpcRequest(String json) { super(json); }
        public static PerformRpcRequest fromJson(String json) { return new PerformRpcRequest(json); }
    }
    public static final class PerformRpcResponse extends ProtocolModel {
        public PerformRpcResponse() { super("{}"); }
        public PerformRpcResponse(String json) { super(json); }
        public static PerformRpcResponse fromJson(String json) { return new PerformRpcResponse(json); }
    }
    public static final class StartEgressRequest extends ProtocolModel {
        public StartEgressRequest() { super("{}"); }
        public StartEgressRequest(String json) { super(json); }
        public static StartEgressRequest fromJson(String json) { return new StartEgressRequest(json); }
    }
    public static final class EgressInfo extends ProtocolModel {
        public EgressInfo() { super("{}"); }
        public EgressInfo(String json) { super(json); }
        public static EgressInfo fromJson(String json) { return new EgressInfo(json); }
    }
    public static final class RoomCompositeEgressRequest extends ProtocolModel {
        public RoomCompositeEgressRequest() { super("{}"); }
        public RoomCompositeEgressRequest(String json) { super(json); }
        public static RoomCompositeEgressRequest fromJson(String json) { return new RoomCompositeEgressRequest(json); }
    }
    public static final class WebEgressRequest extends ProtocolModel {
        public WebEgressRequest() { super("{}"); }
        public WebEgressRequest(String json) { super(json); }
        public static WebEgressRequest fromJson(String json) { return new WebEgressRequest(json); }
    }
    public static final class ParticipantEgressRequest extends ProtocolModel {
        public ParticipantEgressRequest() { super("{}"); }
        public ParticipantEgressRequest(String json) { super(json); }
        public static ParticipantEgressRequest fromJson(String json) { return new ParticipantEgressRequest(json); }
    }
    public static final class TrackCompositeEgressRequest extends ProtocolModel {
        public TrackCompositeEgressRequest() { super("{}"); }
        public TrackCompositeEgressRequest(String json) { super(json); }
        public static TrackCompositeEgressRequest fromJson(String json) { return new TrackCompositeEgressRequest(json); }
    }
    public static final class TrackEgressRequest extends ProtocolModel {
        public TrackEgressRequest() { super("{}"); }
        public TrackEgressRequest(String json) { super(json); }
        public static TrackEgressRequest fromJson(String json) { return new TrackEgressRequest(json); }
    }
    public static final class UpdateLayoutRequest extends ProtocolModel {
        public UpdateLayoutRequest() { super("{}"); }
        public UpdateLayoutRequest(String json) { super(json); }
        public static UpdateLayoutRequest fromJson(String json) { return new UpdateLayoutRequest(json); }
    }
    public static final class UpdateStreamRequest extends ProtocolModel {
        public UpdateStreamRequest() { super("{}"); }
        public UpdateStreamRequest(String json) { super(json); }
        public static UpdateStreamRequest fromJson(String json) { return new UpdateStreamRequest(json); }
    }
    public static final class ListEgressRequest extends ProtocolModel {
        public ListEgressRequest() { super("{}"); }
        public ListEgressRequest(String json) { super(json); }
        public static ListEgressRequest fromJson(String json) { return new ListEgressRequest(json); }
    }
    public static final class ListEgressResponse extends ProtocolModel {
        public ListEgressResponse() { super("{}"); }
        public ListEgressResponse(String json) { super(json); }
        public static ListEgressResponse fromJson(String json) { return new ListEgressResponse(json); }
    }
    public static final class StopEgressRequest extends ProtocolModel {
        public StopEgressRequest() { super("{}"); }
        public StopEgressRequest(String json) { super(json); }
        public static StopEgressRequest fromJson(String json) { return new StopEgressRequest(json); }
    }
    public static final class CreateIngressRequest extends ProtocolModel {
        public CreateIngressRequest() { super("{}"); }
        public CreateIngressRequest(String json) { super(json); }
        public static CreateIngressRequest fromJson(String json) { return new CreateIngressRequest(json); }
    }
    public static final class IngressInfo extends ProtocolModel {
        public IngressInfo() { super("{}"); }
        public IngressInfo(String json) { super(json); }
        public static IngressInfo fromJson(String json) { return new IngressInfo(json); }
    }
    public static final class UpdateIngressRequest extends ProtocolModel {
        public UpdateIngressRequest() { super("{}"); }
        public UpdateIngressRequest(String json) { super(json); }
        public static UpdateIngressRequest fromJson(String json) { return new UpdateIngressRequest(json); }
    }
    public static final class ListIngressRequest extends ProtocolModel {
        public ListIngressRequest() { super("{}"); }
        public ListIngressRequest(String json) { super(json); }
        public static ListIngressRequest fromJson(String json) { return new ListIngressRequest(json); }
    }
    public static final class ListIngressResponse extends ProtocolModel {
        public ListIngressResponse() { super("{}"); }
        public ListIngressResponse(String json) { super(json); }
        public static ListIngressResponse fromJson(String json) { return new ListIngressResponse(json); }
    }
    public static final class DeleteIngressRequest extends ProtocolModel {
        public DeleteIngressRequest() { super("{}"); }
        public DeleteIngressRequest(String json) { super(json); }
        public static DeleteIngressRequest fromJson(String json) { return new DeleteIngressRequest(json); }
    }
    public static final class CreateSIPInboundTrunkRequest extends ProtocolModel {
        public CreateSIPInboundTrunkRequest() { super("{}"); }
        public CreateSIPInboundTrunkRequest(String json) { super(json); }
        public static CreateSIPInboundTrunkRequest fromJson(String json) { return new CreateSIPInboundTrunkRequest(json); }
    }
    public static final class SIPInboundTrunkInfo extends ProtocolModel {
        public SIPInboundTrunkInfo() { super("{}"); }
        public SIPInboundTrunkInfo(String json) { super(json); }
        public static SIPInboundTrunkInfo fromJson(String json) { return new SIPInboundTrunkInfo(json); }
    }
    public static final class CreateSIPOutboundTrunkRequest extends ProtocolModel {
        public CreateSIPOutboundTrunkRequest() { super("{}"); }
        public CreateSIPOutboundTrunkRequest(String json) { super(json); }
        public static CreateSIPOutboundTrunkRequest fromJson(String json) { return new CreateSIPOutboundTrunkRequest(json); }
    }
    public static final class SIPOutboundTrunkInfo extends ProtocolModel {
        public SIPOutboundTrunkInfo() { super("{}"); }
        public SIPOutboundTrunkInfo(String json) { super(json); }
        public static SIPOutboundTrunkInfo fromJson(String json) { return new SIPOutboundTrunkInfo(json); }
    }
    public static final class UpdateSIPInboundTrunkRequest extends ProtocolModel {
        public UpdateSIPInboundTrunkRequest() { super("{}"); }
        public UpdateSIPInboundTrunkRequest(String json) { super(json); }
        public static UpdateSIPInboundTrunkRequest fromJson(String json) { return new UpdateSIPInboundTrunkRequest(json); }
    }
    public static final class UpdateSIPOutboundTrunkRequest extends ProtocolModel {
        public UpdateSIPOutboundTrunkRequest() { super("{}"); }
        public UpdateSIPOutboundTrunkRequest(String json) { super(json); }
        public static UpdateSIPOutboundTrunkRequest fromJson(String json) { return new UpdateSIPOutboundTrunkRequest(json); }
    }
    public static final class GetSIPInboundTrunkRequest extends ProtocolModel {
        public GetSIPInboundTrunkRequest() { super("{}"); }
        public GetSIPInboundTrunkRequest(String json) { super(json); }
        public static GetSIPInboundTrunkRequest fromJson(String json) { return new GetSIPInboundTrunkRequest(json); }
    }
    public static final class GetSIPInboundTrunkResponse extends ProtocolModel {
        public GetSIPInboundTrunkResponse() { super("{}"); }
        public GetSIPInboundTrunkResponse(String json) { super(json); }
        public static GetSIPInboundTrunkResponse fromJson(String json) { return new GetSIPInboundTrunkResponse(json); }
    }
    public static final class GetSIPOutboundTrunkRequest extends ProtocolModel {
        public GetSIPOutboundTrunkRequest() { super("{}"); }
        public GetSIPOutboundTrunkRequest(String json) { super(json); }
        public static GetSIPOutboundTrunkRequest fromJson(String json) { return new GetSIPOutboundTrunkRequest(json); }
    }
    public static final class GetSIPOutboundTrunkResponse extends ProtocolModel {
        public GetSIPOutboundTrunkResponse() { super("{}"); }
        public GetSIPOutboundTrunkResponse(String json) { super(json); }
        public static GetSIPOutboundTrunkResponse fromJson(String json) { return new GetSIPOutboundTrunkResponse(json); }
    }
    public static final class ListSIPTrunkRequest extends ProtocolModel {
        public ListSIPTrunkRequest() { super("{}"); }
        public ListSIPTrunkRequest(String json) { super(json); }
        public static ListSIPTrunkRequest fromJson(String json) { return new ListSIPTrunkRequest(json); }
    }
    public static final class ListSIPTrunkResponse extends ProtocolModel {
        public ListSIPTrunkResponse() { super("{}"); }
        public ListSIPTrunkResponse(String json) { super(json); }
        public static ListSIPTrunkResponse fromJson(String json) { return new ListSIPTrunkResponse(json); }
    }
    public static final class ListSIPInboundTrunkRequest extends ProtocolModel {
        public ListSIPInboundTrunkRequest() { super("{}"); }
        public ListSIPInboundTrunkRequest(String json) { super(json); }
        public static ListSIPInboundTrunkRequest fromJson(String json) { return new ListSIPInboundTrunkRequest(json); }
    }
    public static final class ListSIPInboundTrunkResponse extends ProtocolModel {
        public ListSIPInboundTrunkResponse() { super("{}"); }
        public ListSIPInboundTrunkResponse(String json) { super(json); }
        public static ListSIPInboundTrunkResponse fromJson(String json) { return new ListSIPInboundTrunkResponse(json); }
    }
    public static final class ListSIPOutboundTrunkRequest extends ProtocolModel {
        public ListSIPOutboundTrunkRequest() { super("{}"); }
        public ListSIPOutboundTrunkRequest(String json) { super(json); }
        public static ListSIPOutboundTrunkRequest fromJson(String json) { return new ListSIPOutboundTrunkRequest(json); }
    }
    public static final class ListSIPOutboundTrunkResponse extends ProtocolModel {
        public ListSIPOutboundTrunkResponse() { super("{}"); }
        public ListSIPOutboundTrunkResponse(String json) { super(json); }
        public static ListSIPOutboundTrunkResponse fromJson(String json) { return new ListSIPOutboundTrunkResponse(json); }
    }
    public static final class DeleteSIPTrunkRequest extends ProtocolModel {
        public DeleteSIPTrunkRequest() { super("{}"); }
        public DeleteSIPTrunkRequest(String json) { super(json); }
        public static DeleteSIPTrunkRequest fromJson(String json) { return new DeleteSIPTrunkRequest(json); }
    }
    public static final class SIPTrunkInfo extends ProtocolModel {
        public SIPTrunkInfo() { super("{}"); }
        public SIPTrunkInfo(String json) { super(json); }
        public static SIPTrunkInfo fromJson(String json) { return new SIPTrunkInfo(json); }
    }
    public static final class CreateSIPDispatchRuleRequest extends ProtocolModel {
        public CreateSIPDispatchRuleRequest() { super("{}"); }
        public CreateSIPDispatchRuleRequest(String json) { super(json); }
        public static CreateSIPDispatchRuleRequest fromJson(String json) { return new CreateSIPDispatchRuleRequest(json); }
    }
    public static final class SIPDispatchRuleInfo extends ProtocolModel {
        public SIPDispatchRuleInfo() { super("{}"); }
        public SIPDispatchRuleInfo(String json) { super(json); }
        public static SIPDispatchRuleInfo fromJson(String json) { return new SIPDispatchRuleInfo(json); }
    }
    public static final class UpdateSIPDispatchRuleRequest extends ProtocolModel {
        public UpdateSIPDispatchRuleRequest() { super("{}"); }
        public UpdateSIPDispatchRuleRequest(String json) { super(json); }
        public static UpdateSIPDispatchRuleRequest fromJson(String json) { return new UpdateSIPDispatchRuleRequest(json); }
    }
    public static final class ListSIPDispatchRuleRequest extends ProtocolModel {
        public ListSIPDispatchRuleRequest() { super("{}"); }
        public ListSIPDispatchRuleRequest(String json) { super(json); }
        public static ListSIPDispatchRuleRequest fromJson(String json) { return new ListSIPDispatchRuleRequest(json); }
    }
    public static final class ListSIPDispatchRuleResponse extends ProtocolModel {
        public ListSIPDispatchRuleResponse() { super("{}"); }
        public ListSIPDispatchRuleResponse(String json) { super(json); }
        public static ListSIPDispatchRuleResponse fromJson(String json) { return new ListSIPDispatchRuleResponse(json); }
    }
    public static final class DeleteSIPDispatchRuleRequest extends ProtocolModel {
        public DeleteSIPDispatchRuleRequest() { super("{}"); }
        public DeleteSIPDispatchRuleRequest(String json) { super(json); }
        public static DeleteSIPDispatchRuleRequest fromJson(String json) { return new DeleteSIPDispatchRuleRequest(json); }
    }
    public static final class CreateSIPParticipantRequest extends ProtocolModel {
        public CreateSIPParticipantRequest() { super("{}"); }
        public CreateSIPParticipantRequest(String json) { super(json); }
        public static CreateSIPParticipantRequest fromJson(String json) { return new CreateSIPParticipantRequest(json); }
    }
    public static final class SIPParticipantInfo extends ProtocolModel {
        public SIPParticipantInfo() { super("{}"); }
        public SIPParticipantInfo(String json) { super(json); }
        public static SIPParticipantInfo fromJson(String json) { return new SIPParticipantInfo(json); }
    }
    public static final class TransferSIPParticipantRequest extends ProtocolModel {
        public TransferSIPParticipantRequest() { super("{}"); }
        public TransferSIPParticipantRequest(String json) { super(json); }
        public static TransferSIPParticipantRequest fromJson(String json) { return new TransferSIPParticipantRequest(json); }
    }
    public static final class CreateAgentDispatchRequest extends ProtocolModel {
        public CreateAgentDispatchRequest() { super("{}"); }
        public CreateAgentDispatchRequest(String json) { super(json); }
        public static CreateAgentDispatchRequest fromJson(String json) { return new CreateAgentDispatchRequest(json); }
    }
    public static final class AgentDispatch extends ProtocolModel {
        public AgentDispatch() { super("{}"); }
        public AgentDispatch(String json) { super(json); }
        public static AgentDispatch fromJson(String json) { return new AgentDispatch(json); }
    }
    public static final class DeleteAgentDispatchRequest extends ProtocolModel {
        public DeleteAgentDispatchRequest() { super("{}"); }
        public DeleteAgentDispatchRequest(String json) { super(json); }
        public static DeleteAgentDispatchRequest fromJson(String json) { return new DeleteAgentDispatchRequest(json); }
    }
    public static final class ListAgentDispatchRequest extends ProtocolModel {
        public ListAgentDispatchRequest() { super("{}"); }
        public ListAgentDispatchRequest(String json) { super(json); }
        public static ListAgentDispatchRequest fromJson(String json) { return new ListAgentDispatchRequest(json); }
    }
    public static final class ListAgentDispatchResponse extends ProtocolModel {
        public ListAgentDispatchResponse() { super("{}"); }
        public ListAgentDispatchResponse(String json) { super(json); }
        public static ListAgentDispatchResponse fromJson(String json) { return new ListAgentDispatchResponse(json); }
    }
    public static final class DialWhatsAppCallRequest extends ProtocolModel {
        public DialWhatsAppCallRequest() { super("{}"); }
        public DialWhatsAppCallRequest(String json) { super(json); }
        public static DialWhatsAppCallRequest fromJson(String json) { return new DialWhatsAppCallRequest(json); }
    }
    public static final class DialWhatsAppCallResponse extends ProtocolModel {
        public DialWhatsAppCallResponse() { super("{}"); }
        public DialWhatsAppCallResponse(String json) { super(json); }
        public static DialWhatsAppCallResponse fromJson(String json) { return new DialWhatsAppCallResponse(json); }
    }
    public static final class DisconnectWhatsAppCallRequest extends ProtocolModel {
        public DisconnectWhatsAppCallRequest() { super("{}"); }
        public DisconnectWhatsAppCallRequest(String json) { super(json); }
        public static DisconnectWhatsAppCallRequest fromJson(String json) { return new DisconnectWhatsAppCallRequest(json); }
    }
    public static final class DisconnectWhatsAppCallResponse extends ProtocolModel {
        public DisconnectWhatsAppCallResponse() { super("{}"); }
        public DisconnectWhatsAppCallResponse(String json) { super(json); }
        public static DisconnectWhatsAppCallResponse fromJson(String json) { return new DisconnectWhatsAppCallResponse(json); }
    }
    public static final class ConnectWhatsAppCallRequest extends ProtocolModel {
        public ConnectWhatsAppCallRequest() { super("{}"); }
        public ConnectWhatsAppCallRequest(String json) { super(json); }
        public static ConnectWhatsAppCallRequest fromJson(String json) { return new ConnectWhatsAppCallRequest(json); }
    }
    public static final class ConnectWhatsAppCallResponse extends ProtocolModel {
        public ConnectWhatsAppCallResponse() { super("{}"); }
        public ConnectWhatsAppCallResponse(String json) { super(json); }
        public static ConnectWhatsAppCallResponse fromJson(String json) { return new ConnectWhatsAppCallResponse(json); }
    }
    public static final class AcceptWhatsAppCallRequest extends ProtocolModel {
        public AcceptWhatsAppCallRequest() { super("{}"); }
        public AcceptWhatsAppCallRequest(String json) { super(json); }
        public static AcceptWhatsAppCallRequest fromJson(String json) { return new AcceptWhatsAppCallRequest(json); }
    }
    public static final class AcceptWhatsAppCallResponse extends ProtocolModel {
        public AcceptWhatsAppCallResponse() { super("{}"); }
        public AcceptWhatsAppCallResponse(String json) { super(json); }
        public static AcceptWhatsAppCallResponse fromJson(String json) { return new AcceptWhatsAppCallResponse(json); }
    }
    public static final class ConnectTwilioCallRequest extends ProtocolModel {
        public ConnectTwilioCallRequest() { super("{}"); }
        public ConnectTwilioCallRequest(String json) { super(json); }
        public static ConnectTwilioCallRequest fromJson(String json) { return new ConnectTwilioCallRequest(json); }
    }
    public static final class ConnectTwilioCallResponse extends ProtocolModel {
        public ConnectTwilioCallResponse() { super("{}"); }
        public ConnectTwilioCallResponse(String json) { super(json); }
        public static ConnectTwilioCallResponse fromJson(String json) { return new ConnectTwilioCallResponse(json); }
    }
}
