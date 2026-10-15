package io.livekit.server;

import static io.livekit.server.Models.*;

public final class IngressClient {
    private final LiveKitApi api;
    IngressClient(LiveKitApi api) { this.api = api; }
    public IngressInfo createIngress(CreateIngressRequest request) {
        return new IngressInfo(api.call("Ingress.CreateIngress", request));
    }
    public IngressInfo updateIngress(UpdateIngressRequest request) {
        return new IngressInfo(api.call("Ingress.UpdateIngress", request));
    }
    public ListIngressResponse listIngress(ListIngressRequest request) {
        return new ListIngressResponse(api.call("Ingress.ListIngress", request));
    }
    public IngressInfo deleteIngress(DeleteIngressRequest request) {
        return new IngressInfo(api.call("Ingress.DeleteIngress", request));
    }
}
