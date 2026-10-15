package io.livekit.server;

import static io.livekit.server.Models.*;

import java.io.ByteArrayOutputStream;
import java.lang.reflect.Modifier;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.time.Duration;
import java.time.Instant;
import java.util.Arrays;
import java.util.Base64;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.atomic.AtomicInteger;
import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

/** Deterministic, offline JNI contract tests. No server or physical devices are accessed. */
public final class OfflineSmoke {
    private static final String KEY = "binding-test-key";
    private static final String SECRET = "binding-test-secret-at-least-32-characters";
    private static int checks;
    private static void check(boolean condition, String message) {
        checks++;
        if (!condition) throw new AssertionError(message);
    }
    private static LiveKitException failure(Runnable call, LiveKitException.ErrorCode code) {
        try { call.run(); } catch (LiveKitException error) {
            check(error.errorCode() == code, "error code: " + error);
            return error;
        }
        throw new AssertionError("expected " + code);
    }
    private static byte[] bytes(String text) { return text.getBytes(StandardCharsets.UTF_8); }
    private static String b64(byte[] bytes) { return Base64.getUrlEncoder().withoutPadding().encodeToString(bytes); }
    private static byte[] hmac(String input) throws Exception {
        var mac = Mac.getInstance("HmacSHA256");
        mac.init(new SecretKeySpec(bytes(SECRET), "HmacSHA256"));
        return mac.doFinal(bytes(input));
    }
    private static String sign(String claims) throws Exception {
        String input = b64(bytes("{\"alg\":\"HS256\",\"typ\":\"JWT\"}")) + "." + b64(bytes(claims));
        return input + "." + b64(hmac(input));
    }
    private static LiveKitApi api(HttpTransport transport) {
        return new LiveKitApi(new ApiOptions("ws://offline.invalid/", KEY, SECRET, "",
            Duration.ofMillis(1234), transport));
    }
    private static String header(String[] pairs, String key) {
        for (int i = 0; i < pairs.length; i += 2) if (pairs[i].equals(key)) return pairs[i + 1];
        return null;
    }
    private static void tokens() throws Exception {
        VideoGrant grant = new VideoGrant();
        grant.roomJoin = true;
        grant.room = "房间😀";
        grant.canPublish = false;
        grant.canPublishSources = List.of("microphone");
        SipGrant sip = new SipGrant(); sip.call = true;
        AgentGrant agent = new AgentGrant(); agent.simulationAdmin = true;
        String token = new AccessToken(KEY, SECRET).setIdentity("用户😀")
            .setName("display").setMetadata("quote\"\n\u0000")
            .setAttributes(Map.of("语言", "中文😀")).setValidFor(Duration.ofSeconds(60))
            .setVideoGrant(grant).setSipGrant(sip).setAgentGrant(agent)
            .toJwt(Instant.ofEpochSecond(1700000000));
        String[] parts = token.split("\\.");
        check(Arrays.equals(Base64.getUrlDecoder().decode(parts[2]), hmac(parts[0] + "." + parts[1])), "JWT HMAC");
        String claims = new String(Base64.getUrlDecoder().decode(parts[1]), StandardCharsets.UTF_8);
        for (String expected : List.of("用户😀", "房间😀", "中文😀", "\"canPublish\":false", "\"exp\":1700000060",
                "\"simulationAdmin\":true", "\"call\":true", "\\u0000")) {
            check(claims.contains(expected), "JWT claim " + expected);
        }
        check(!claims.contains("canSubscribe"), "absent optional grant");
        failure(() -> new AccessToken("", "").toJwt(), LiveKitException.ErrorCode.AUTHENTICATION);
        failure(() -> new AccessToken(KEY, SECRET).setValidFor(Duration.ofSeconds(Long.MAX_VALUE)).toJwt(),
            LiveKitException.ErrorCode.INVALID_ARGUMENT);
    }
    private static void services() throws Exception {
        AtomicInteger calls = new AtomicInteger();
        String[] expectedPath = new String[1];
        var api = api((url, method, headers, body, timeout) -> {
            check(url.equals("http://offline.invalid/twirp/livekit." + expectedPath[0]), "RPC path " + url);
            check(method.equals("POST") && timeout == 1234, "HTTP method and timeout");
            check("application/protobuf".equals(header(headers, "Content-Type")), "binary transport");
            check(header(headers, "Authorization").startsWith("Bearer "), "request authentication");
            calls.incrementAndGet();
            return new HttpResponse(200, new byte[0]);
        });
        for (Object service : List.of(api.room(), api.egress(), api.ingress(), api.sip(), api.agentDispatch(), api.connector())) {
            String serviceName = service instanceof RoomServiceClient ? "RoomService"
                : service instanceof AgentDispatchClient ? "AgentDispatchService"
                : service instanceof SipClient ? "SIP" : service.getClass().getSimpleName().replace("Client", "");
            for (var method : service.getClass().getDeclaredMethods()) {
                if (!Modifier.isPublic(method.getModifiers()) || method.getParameterCount() != 1
                        || !ProtocolModel.class.isAssignableFrom(method.getParameterTypes()[0])) continue;
                String rpc = service instanceof SipClient
                    ? method.getParameterTypes()[0].getSimpleName().replace("Request", "")
                    : Character.toUpperCase(method.getName().charAt(0)) + method.getName().substring(1);
                expectedPath[0] = serviceName + "/" + rpc;
                Object request = method.getParameterTypes()[0].getConstructor().newInstance();
                check(method.invoke(service, request) instanceof ProtocolModel, "typed response");
            }
        }
        check(calls.get() == 52, "all 52 service methods invoked");
        expectedPath[0] = "AgentDispatchService/ListDispatch";
        check(api.agentDispatch().getDispatch("missing", "room") == null, "missing dispatch");
        expectedPath[0] = "RoomService/ListRooms";
        check(api.room().listRooms() != null, "list rooms convenience");
        String name = "房间😀";
        var echo = api((url, method, headers, body, timeout) -> {
            // CreateRoomRequest.name is protobuf field 1; Room.name is field 2.
            byte[] text = bytes(name);
            check(body[0] == 10 && body[1] == text.length, "protobuf request field");
            check(Arrays.equals(Arrays.copyOfRange(body, 2, body.length), text), "UTF-8 request");
            var out = new ByteArrayOutputStream();
            out.write(18); out.write(text.length); out.writeBytes(text);
            return new HttpResponse(200, out.toByteArray());
        });
        check(echo.room().createRoom(new CreateRoomRequest("{\"name\":\"" + name + "\"}")).json().contains(name), "UTF-8 response");
        var http = failure(() -> api((u, m, h, b, t) -> new HttpResponse(403,
            bytes("{\"code\":\"permission_denied\",\"msg\":\"拒绝😀\"}"))).room().listRooms(),
            LiveKitException.ErrorCode.HTTP);
        check(http.httpStatus() == 403 && http.twirpCode().equals("permission_denied")
            && http.getMessage().equals("拒绝😀"), "native error details");
        failure(() -> api.room().createRoom(new CreateRoomRequest("invalid json")), LiveKitException.ErrorCode.PROTOCOL);
        failure(() -> api((u, m, h, b, t) -> new HttpResponse(200, new byte[]{-1})).room().listRooms(),
            LiveKitException.ErrorCode.PROTOCOL);
        var sentinel = new IllegalStateException("transport exception");
        try {
            api((u, m, h, b, t) -> { throw sentinel; }).room().listRooms();
            throw new AssertionError("transport exception missing");
        } catch (IllegalStateException error) { check(error == sentinel, "Java exception preserved"); }
        var concurrent = api((u, m, h, b, t) -> new HttpResponse(200, new byte[0]));
        CompletableFuture.allOf(java.util.stream.IntStream.range(0, 24)
            .mapToObj(i -> CompletableFuture.runAsync(() -> concurrent.room().listRooms()))
            .toArray(CompletableFuture<?>[]::new)).join();
        check(true, "concurrent JNI calls");
    }
    private static void webhooks() throws Exception {
        String body = "{\"event\":\"room_started\",\"id\":\"event-😀\",\"createdAt\":\"1700000000\","
            + "\"room\":{\"name\":\"房间😀\"},"
            + "\"participant\":{\"sid\":\"PA_test\",\"identity\":\"用户😀\",\"name\":\"name\",\"metadata\":\"m\"},"
            + "\"track\":{\"sid\":\"TR_test\",\"name\":\"track\"},"
            + "\"egressInfo\":{\"egressId\":\"EG_test\",\"roomId\":\"RM_test\",\"roomName\":\"房间😀\"},"
            + "\"ingressInfo\":{\"ingressId\":\"IN_test\",\"name\":\"input\",\"roomName\":\"房间😀\"},"
            + "\"futureField\":true}";
        String checksum = Base64.getEncoder().encodeToString(MessageDigest.getInstance("SHA-256").digest(bytes(body)));
        String auth = sign("{\"iss\":\"" + KEY + "\",\"sha256\":\"" + checksum + "\"}");
        var receiver = new WebhookReceiver(Map.of("old-key", "old-secret", KEY, SECRET));
        var event = receiver.receive(bytes(body), "Bearer " + auth);
        check(event.event().equals("room_started") && event.id().equals("event-😀")
            && event.createdAt() == 1700000000L, "verified event fields");
        check(event.rawBody().equals(body), "exact raw webhook including future fields");
        check(event.room().name().equals("房间😀") && event.participant().identity().equals("用户😀")
            && event.track().sid().equals("TR_test") && event.egress().egressId().equals("EG_test")
            && event.ingress().ingressId().equals("IN_test"), "typed webhook children");
        check(new WebhookReceiver(key -> key.equals(KEY) ? SECRET : null).receive(body, auth).equals(event),
            "dynamic key provider");
        failure(() -> new WebhookReceiver(key -> null).receive(body, auth), LiveKitException.ErrorCode.AUTHENTICATION);
        var sentinel = new IllegalStateException("key lookup failure");
        try {
            new WebhookReceiver(key -> { throw sentinel; }).receive(body, auth);
            throw new AssertionError("key provider exception missing");
        } catch (IllegalStateException error) { check(error == sentinel, "key provider exception preserved"); }
        AtomicInteger delivered = new AtomicInteger();
        receiver.setCallback(value -> delivered.incrementAndGet());
        receiver.receiveAndDispatch(body, auth);
        check(delivered.get() == 1, "callback delivery");
        failure(() -> receiver.receiveAndDispatch(body + " ", auth), LiveKitException.ErrorCode.AUTHENTICATION);
        check(delivered.get() == 1, "invalid body never dispatched");
        failure(() -> new WebhookReceiver(KEY, "wrong secret").receive(body, auth), LiveKitException.ErrorCode.AUTHENTICATION);
        failure(() -> receiver.receive(body, ""), LiveKitException.ErrorCode.AUTHENTICATION);
    }
    public static void main(String[] args) throws Exception {
        tokens();
        services();
        webhooks();
        System.out.println("Offline JNI smoke passed: " + checks + " checks");
    }
}
