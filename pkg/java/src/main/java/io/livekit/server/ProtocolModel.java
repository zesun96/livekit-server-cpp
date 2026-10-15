package io.livekit.server;

import java.util.Objects;

/** Immutable canonical protobuf JSON. Validation occurs in the native SDK on use. */
public abstract class ProtocolModel {
    private final String json;
    protected ProtocolModel(String json) { this.json = Objects.requireNonNull(json, "json"); }
    public final String json() { return json; }
    public final boolean isEmpty() { return json.isEmpty() || json.equals("{}"); }
    @Override public final String toString() { return json; }
    @Override public final int hashCode() { return Objects.hash(getClass(), json); }
    @Override public final boolean equals(Object other) {
        return other != null && other.getClass() == getClass()
            && json.equals(((ProtocolModel) other).json);
    }
}
