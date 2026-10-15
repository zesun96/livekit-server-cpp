package io.livekit.server;

import java.nio.charset.StandardCharsets;
import java.util.Objects;

final class Native {
    static {
        String path = System.getProperty("livekit.server.java.library");
        if (path == null || path.isBlank()) System.loadLibrary("livekitserver_java");
        else System.load(path);
    }
    private Native() {}
    static byte[] bytes(String text) {
        return Objects.requireNonNull(text, "text").getBytes(StandardCharsets.UTF_8);
    }
    static String text(byte[] bytes) {
        return bytes == null ? null : new String(bytes, StandardCharsets.UTF_8);
    }
    static native byte[] call(byte[] options, String operation, byte[] request, HttpTransport transport);
    static native byte[] token(byte[] options, long epochSeconds);
    static native WebhookEvent receive(byte[] secrets, byte[] body, byte[] authorization,
                                      java.util.function.Function<String, String> keyProvider);
    static native byte[] getDispatch(byte[] options, byte[] id, byte[] room, HttpTransport transport);
}
