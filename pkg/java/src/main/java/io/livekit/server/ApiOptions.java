package io.livekit.server;

import java.time.Duration;
import java.util.Map;
import java.util.Objects;

/** Immutable options. Empty credentials/URL use the C++ SDK's LIVEKIT_* environment fallback. */
public record ApiOptions(String url, String apiKey, String apiSecret, String accessToken,
                         Duration timeout, HttpTransport transport) {
    public ApiOptions {
        Objects.requireNonNull(url, "url");
        Objects.requireNonNull(apiKey, "apiKey");
        Objects.requireNonNull(apiSecret, "apiSecret");
        Objects.requireNonNull(accessToken, "accessToken");
        Objects.requireNonNull(timeout, "timeout");
        if (timeout.toMillis() <= 0) throw new IllegalArgumentException("timeout must be positive");
    }
    public ApiOptions(String url, String apiKey, String apiSecret) {
        this(url, apiKey, apiSecret, "", Duration.ofSeconds(30), null);
    }
    public ApiOptions() { this("", "", ""); }
    byte[] encode() {
        return Native.bytes(Json.encode(Map.of("url", url, "apiKey", apiKey, "apiSecret", apiSecret,
            "accessToken", accessToken, "timeoutMs", Long.toString(timeout.toMillis()))));
    }
    @Override public String toString() { return "ApiOptions[credentials redacted]"; }
}
