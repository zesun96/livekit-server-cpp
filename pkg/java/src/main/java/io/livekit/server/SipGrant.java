package io.livekit.server;

/** Mutable token grant builder. Null optional booleans retain server defaults. */
public final class SipGrant {
    public boolean admin = false;
    public boolean call = false;
    java.util.Map<String, Object> fields() {
        var fields = new java.util.LinkedHashMap<String, Object>();
        fields.put("admin", admin);
        fields.put("call", call);
        return fields;
    }
}
