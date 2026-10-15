package io.livekit.server;

/** Verified event with complete original JSON, including future protocol fields. */
public record WebhookEvent(String event, String id, long createdAt, Room room,
                           Participant participant, Track track, Egress egress,
                           Ingress ingress, String rawBody) {
    public record Room(String sid, String name, String metadata) {}
    public record Participant(String sid, String identity, String name, String metadata) {}
    public record Track(String sid, String name) {}
    public record Egress(String egressId, String roomId, String roomName) {}
    public record Ingress(String ingressId, String name, String roomName) {}
}
