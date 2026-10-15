package io.livekit.server;

import static io.livekit.server.Models.*;

public final class ConnectorClient {
    private final LiveKitApi api;
    ConnectorClient(LiveKitApi api) { this.api = api; }
    public DialWhatsAppCallResponse dialWhatsAppCall(DialWhatsAppCallRequest request) {
        return new DialWhatsAppCallResponse(api.call("Connector.DialWhatsAppCall", request));
    }
    public DisconnectWhatsAppCallResponse disconnectWhatsAppCall(DisconnectWhatsAppCallRequest request) {
        return new DisconnectWhatsAppCallResponse(api.call("Connector.DisconnectWhatsAppCall", request));
    }
    public ConnectWhatsAppCallResponse connectWhatsAppCall(ConnectWhatsAppCallRequest request) {
        return new ConnectWhatsAppCallResponse(api.call("Connector.ConnectWhatsAppCall", request));
    }
    public AcceptWhatsAppCallResponse acceptWhatsAppCall(AcceptWhatsAppCallRequest request) {
        return new AcceptWhatsAppCallResponse(api.call("Connector.AcceptWhatsAppCall", request));
    }
    public ConnectTwilioCallResponse connectTwilioCall(ConnectTwilioCallRequest request) {
        return new ConnectTwilioCallResponse(api.call("Connector.ConnectTwilioCall", request));
    }
}
