package io.livekit.server;

import java.util.Objects;

public record HttpResponse(int statusCode, byte[] body) {
    public HttpResponse { body = Objects.requireNonNull(body, "body").clone(); }
    @Override public byte[] body() { return body.clone(); }
}
