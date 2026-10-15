package io.livekit.server;

import java.util.Objects;

/** Synchronous server API. Calls own their native state and require no explicit cleanup.
 * Use CompletableFuture.supplyAsync with an application executor for asynchronous calls.
 */
public final class LiveKitApi {
    private final ApiOptions options;
    private final RoomServiceClient room = new RoomServiceClient(this);
    private final EgressClient egress = new EgressClient(this);
    private final IngressClient ingress = new IngressClient(this);
    private final SipClient sip = new SipClient(this);
    private final AgentDispatchClient agentDispatch = new AgentDispatchClient(this);
    private final ConnectorClient connector = new ConnectorClient(this);
    public LiveKitApi(ApiOptions options) { this.options = Objects.requireNonNull(options, "options"); }
    public LiveKitApi(String url, String apiKey, String apiSecret) { this(new ApiOptions(url, apiKey, apiSecret)); }
    public LiveKitApi() { this(new ApiOptions()); }
    public RoomServiceClient room() { return room; }
    public EgressClient egress() { return egress; }
    public IngressClient ingress() { return ingress; }
    public SipClient sip() { return sip; }
    public AgentDispatchClient agentDispatch() { return agentDispatch; }
    public ConnectorClient connector() { return connector; }
    String call(String operation, ProtocolModel request) {
        return Native.text(Native.call(options.encode(), operation,
            Native.bytes(Objects.requireNonNull(request, "request").json()), options.transport()));
    }
    String getDispatch(String id, String room) {
        return Native.text(Native.getDispatch(options.encode(), Native.bytes(id), Native.bytes(room), options.transport()));
    }
}
