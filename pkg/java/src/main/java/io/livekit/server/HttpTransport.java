package io.livekit.server;

/** Synchronous transport. Request and response bodies contain binary protobuf, not JSON.
 * Implementations may be called concurrently and exceptions propagate to the caller unchanged.
 */
@FunctionalInterface
public interface HttpTransport {
    HttpResponse send(String url, String method, String[] headerPairs, byte[] body, long timeoutMs);
}
