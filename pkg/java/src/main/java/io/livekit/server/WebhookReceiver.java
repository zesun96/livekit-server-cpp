package io.livekit.server;

import java.util.Map;
import java.util.Objects;
import java.util.function.Consumer;
import java.util.function.Function;

/** Verifies raw HTTP request bytes before invoking application callbacks on the calling thread. */
public final class WebhookReceiver {
    private final Map<String, String> secrets;
    private final Function<String, String> keyProvider;
    private volatile Consumer<WebhookEvent> callback = event -> {};
    public WebhookReceiver(String apiKey, String apiSecret) { this(Map.of(apiKey, apiSecret)); }
    public WebhookReceiver(Map<String, String> apiSecrets) {
        secrets = Map.copyOf(apiSecrets);
        keyProvider = null;
    }
    /** The provider executes during verification; return null for an unknown key. */
    public WebhookReceiver(Function<String, String> keyProvider) {
        secrets = Map.of();
        this.keyProvider = Objects.requireNonNull(keyProvider, "keyProvider");
    }
    public void setCallback(Consumer<WebhookEvent> callback) {
        this.callback = Objects.requireNonNull(callback, "callback");
    }
    public WebhookEvent receive(byte[] body, String authorization) {
        return Native.receive(Native.bytes(Json.encode(secrets)),
            Objects.requireNonNull(body, "body"), Native.bytes(authorization), keyProvider);
    }
    public WebhookEvent receive(String body, String authorization) { return receive(Native.bytes(body), authorization); }
    public void dispatch(WebhookEvent event) { callback.accept(Objects.requireNonNull(event, "event")); }
    public void receiveAndDispatch(byte[] body, String authorization) { dispatch(receive(body, authorization)); }
    public void receiveAndDispatch(String body, String authorization) { dispatch(receive(body, authorization)); }
}
