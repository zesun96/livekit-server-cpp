package io.livekit.server.examples;

import io.livekit.server.LiveKitApi;

/** Reads LIVEKIT_URL and LIVEKIT_TOKEN or LIVEKIT_API_KEY/LIVEKIT_API_SECRET. */
public final class ListRooms {
    public static void main(String[] args) {
        var api = new LiveKitApi();
        System.out.println(api.room().listRooms().json());
    }
}
