package io.livekit.server;

/** Mutable token grant builder. Null optional booleans retain server defaults. */
public final class VideoGrant {
    public boolean roomCreate = false;
    public boolean roomList = false;
    public boolean roomRecord = false;
    public boolean roomAdmin = false;
    public boolean roomJoin = false;
    public String room = "";
    public String destinationRoom = "";
    public boolean ingressAdmin = false;
    public Boolean canPublish = null;
    public Boolean canSubscribe = null;
    public Boolean canPublishData = null;
    public Boolean canUpdateOwnMetadata = null;
    public java.util.List<String> canPublishSources = java.util.List.of();
    public boolean hidden = false;
    public boolean recorder = false;
    public boolean agent = false;
    java.util.Map<String, Object> fields() {
        var fields = new java.util.LinkedHashMap<String, Object>();
        fields.put("roomCreate", roomCreate);
        fields.put("roomList", roomList);
        fields.put("roomRecord", roomRecord);
        fields.put("roomAdmin", roomAdmin);
        fields.put("roomJoin", roomJoin);
        fields.put("room", java.util.Objects.requireNonNull(room, "room"));
        fields.put("destinationRoom", java.util.Objects.requireNonNull(destinationRoom, "destinationRoom"));
        fields.put("ingressAdmin", ingressAdmin);
        if (canPublish != null) fields.put("canPublish", canPublish);
        if (canSubscribe != null) fields.put("canSubscribe", canSubscribe);
        if (canPublishData != null) fields.put("canPublishData", canPublishData);
        if (canUpdateOwnMetadata != null) fields.put("canUpdateOwnMetadata", canUpdateOwnMetadata);
        fields.put("canPublishSources", java.util.List.copyOf(canPublishSources));
        fields.put("hidden", hidden);
        fields.put("recorder", recorder);
        fields.put("agent", agent);
        return fields;
    }
}
