package io.livekit.server;

import static io.livekit.server.Models.*;

public final class AgentDispatchClient {
    private final LiveKitApi api;
    AgentDispatchClient(LiveKitApi api) { this.api = api; }
    public AgentDispatch createDispatch(CreateAgentDispatchRequest request) {
        return new AgentDispatch(api.call("AgentDispatch.CreateDispatch", request));
    }
    public AgentDispatch deleteDispatch(DeleteAgentDispatchRequest request) {
        return new AgentDispatch(api.call("AgentDispatch.DeleteDispatch", request));
    }
    public ListAgentDispatchResponse listDispatch(ListAgentDispatchRequest request) {
        return new ListAgentDispatchResponse(api.call("AgentDispatch.ListDispatch", request));
    }
    /** Returns null when the dispatch does not exist. */
    public AgentDispatch getDispatch(String dispatchId, String room) {
        String json = api.getDispatch(dispatchId, room);
        return json == null ? null : new AgentDispatch(json);
    }
}
