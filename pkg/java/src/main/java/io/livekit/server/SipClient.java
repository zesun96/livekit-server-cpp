package io.livekit.server;

import static io.livekit.server.Models.*;

public final class SipClient {
    private final LiveKitApi api;
    SipClient(LiveKitApi api) { this.api = api; }
    public SIPInboundTrunkInfo createInboundTrunk(CreateSIPInboundTrunkRequest request) {
        return new SIPInboundTrunkInfo(api.call("Sip.CreateInboundTrunk", request));
    }
    public SIPOutboundTrunkInfo createOutboundTrunk(CreateSIPOutboundTrunkRequest request) {
        return new SIPOutboundTrunkInfo(api.call("Sip.CreateOutboundTrunk", request));
    }
    public SIPInboundTrunkInfo updateInboundTrunk(UpdateSIPInboundTrunkRequest request) {
        return new SIPInboundTrunkInfo(api.call("Sip.UpdateInboundTrunk", request));
    }
    public SIPOutboundTrunkInfo updateOutboundTrunk(UpdateSIPOutboundTrunkRequest request) {
        return new SIPOutboundTrunkInfo(api.call("Sip.UpdateOutboundTrunk", request));
    }
    public GetSIPInboundTrunkResponse getInboundTrunk(GetSIPInboundTrunkRequest request) {
        return new GetSIPInboundTrunkResponse(api.call("Sip.GetInboundTrunk", request));
    }
    public GetSIPOutboundTrunkResponse getOutboundTrunk(GetSIPOutboundTrunkRequest request) {
        return new GetSIPOutboundTrunkResponse(api.call("Sip.GetOutboundTrunk", request));
    }
    public ListSIPTrunkResponse listTrunks(ListSIPTrunkRequest request) {
        return new ListSIPTrunkResponse(api.call("Sip.ListTrunks", request));
    }
    public ListSIPInboundTrunkResponse listInboundTrunks(ListSIPInboundTrunkRequest request) {
        return new ListSIPInboundTrunkResponse(api.call("Sip.ListInboundTrunks", request));
    }
    public ListSIPOutboundTrunkResponse listOutboundTrunks(ListSIPOutboundTrunkRequest request) {
        return new ListSIPOutboundTrunkResponse(api.call("Sip.ListOutboundTrunks", request));
    }
    public SIPTrunkInfo deleteTrunk(DeleteSIPTrunkRequest request) {
        return new SIPTrunkInfo(api.call("Sip.DeleteTrunk", request));
    }
    public SIPDispatchRuleInfo createDispatchRule(CreateSIPDispatchRuleRequest request) {
        return new SIPDispatchRuleInfo(api.call("Sip.CreateDispatchRule", request));
    }
    public SIPDispatchRuleInfo updateDispatchRule(UpdateSIPDispatchRuleRequest request) {
        return new SIPDispatchRuleInfo(api.call("Sip.UpdateDispatchRule", request));
    }
    public ListSIPDispatchRuleResponse listDispatchRules(ListSIPDispatchRuleRequest request) {
        return new ListSIPDispatchRuleResponse(api.call("Sip.ListDispatchRules", request));
    }
    public SIPDispatchRuleInfo deleteDispatchRule(DeleteSIPDispatchRuleRequest request) {
        return new SIPDispatchRuleInfo(api.call("Sip.DeleteDispatchRule", request));
    }
    public SIPParticipantInfo createParticipant(CreateSIPParticipantRequest request) {
        return new SIPParticipantInfo(api.call("Sip.CreateParticipant", request));
    }
    public Empty transferParticipant(TransferSIPParticipantRequest request) {
        return new Empty(api.call("Sip.TransferParticipant", request));
    }
}
