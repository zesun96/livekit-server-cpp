package io.livekit.server;

import java.time.Duration;
import java.time.Instant;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Objects;

/** Mutable token builder. Signing is performed by the C++ SDK. */
public final class AccessToken {
    private final Map<String, Object> fields = new LinkedHashMap<>();
    public AccessToken(String apiKey, String apiSecret) {
        fields.put("apiKey", Objects.requireNonNull(apiKey, "apiKey"));
        fields.put("apiSecret", Objects.requireNonNull(apiSecret, "apiSecret"));
    }
    public AccessToken setIdentity(String identity) { return set("identity", identity); }
    public AccessToken setName(String name) { return set("name", name); }
    public AccessToken setMetadata(String metadata) { return set("metadata", metadata); }
    public AccessToken setAttributes(Map<String, String> attributes) { return set("attributes", Map.copyOf(attributes)); }
    public AccessToken setValidFor(Duration duration) {
        long seconds = duration.getSeconds();
        if (seconds <= 0) throw new IllegalArgumentException("token lifetime must be positive in seconds");
        return set("validFor", Long.toString(seconds));
    }
    public AccessToken setVideoGrant(VideoGrant grant) { return set("video", grant.fields()); }
    public AccessToken setSipGrant(SipGrant grant) { return set("sip", grant.fields()); }
    public AccessToken setAgentGrant(AgentGrant grant) { return set("agent", grant.fields()); }
    private AccessToken set(String key, Object value) {
        fields.put(key, Objects.requireNonNull(value, key));
        return this;
    }
    public String toJwt() { return toJwt(Instant.now()); }
    public String toJwt(Instant now) {
        return Native.text(Native.token(Native.bytes(Json.encode(fields)), now.getEpochSecond()));
    }
}
