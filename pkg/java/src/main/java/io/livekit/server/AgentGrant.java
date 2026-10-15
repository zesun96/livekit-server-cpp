package io.livekit.server;

/** Mutable token grant builder. Null optional booleans retain server defaults. */
public final class AgentGrant {
    public boolean admin = false;
    public boolean simulationAdmin = false;
    java.util.Map<String, Object> fields() {
        var fields = new java.util.LinkedHashMap<String, Object>();
        fields.put("admin", admin);
        fields.put("simulationAdmin", simulationAdmin);
        return fields;
    }
}
