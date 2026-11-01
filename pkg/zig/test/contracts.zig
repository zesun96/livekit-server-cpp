const std = @import("std");
const sdk = @import("livekit_server");
const options = @import("test_options");
const a = std.testing.allocator;
const Allocator = std.mem.Allocator;
const key = "zig-test-key";
const secret = "zig-test-secret-at-least-32-characters";

fn library() !*sdk.NativeLibrary {
    return sdk.NativeLibrary.open(a, options.native_library orelse return error.MissingNativeTestLibrary);
}
fn client(n: *sdk.NativeLibrary, transport: sdk.Transport) !sdk.LiveKitApi {
    return sdk.LiveKitApi.init(a, n, .{ .url = "ws://offline.invalid/", .api_key = key, .api_secret = secret, .timeout_ms = 1234, .transport = transport });
}
fn b64(allocator: Allocator, bytes: []const u8) ![]const u8 {
    const codec = std.base64.url_safe_no_pad.Encoder;
    return codec.encode(try allocator.alloc(u8, codec.calcSize(bytes.len)), bytes);
}
fn unb64(allocator: Allocator, encoded: []const u8) ![]const u8 {
    const codec = std.base64.url_safe_no_pad.Decoder;
    const bytes = try allocator.alloc(u8, try codec.calcSizeForSlice(encoded));
    errdefer allocator.free(bytes);
    try codec.decode(bytes, encoded);
    return bytes;
}
fn claims(jwt: []const u8) !std.json.Parsed(std.json.Value) {
    var parts = std.mem.splitScalar(u8, jwt, '.');
    _ = parts.next() orelse return error.InvalidJwt;
    const payload = try unb64(a, parts.next() orelse return error.InvalidJwt);
    defer a.free(payload);
    return std.json.parseFromSlice(std.json.Value, a, payload, .{ .allocate = .alloc_always });
}
fn sign(unsigned: []const u8) [32]u8 {
    var signature: [32]u8 = undefined;
    std.crypto.auth.hmac.sha2.HmacSha256.create(&signature, unsigned, secret);
    return signature;
}
fn expectSignature(jwt: []const u8) !void {
    const dot = std.mem.lastIndexOfScalar(u8, jwt, '.') orelse return error.InvalidJwt;
    const bytes = try unb64(a, jwt[dot + 1 ..]);
    defer a.free(bytes);
    try std.testing.expectEqualSlices(u8, &sign(jwt[0..dot]), bytes);
}
fn field(v: std.json.Value, name: []const u8) !std.json.Value {
    if (v != .object) return error.ExpectedJsonObject;
    return v.object.get(name) orelse error.MissingField;
}
fn text(v: std.json.Value, name: []const u8) ![]const u8 {
    const value = try field(v, name);
    if (value != .string) return error.ExpectedString;
    return value.string;
}
fn expectCategory(result: anytype, expected: i32) !void {
    try std.testing.expect(result == .failure);
    try std.testing.expectEqual(expected, result.failure.code);
}
fn empty(_: ?*anyopaque, _: sdk.HttpRequest, _: Allocator) anyerror!sdk.HttpResponse {
    return .{ .status_code = 200 };
}

const Route = struct {
    expected: []const u8,
    calls: usize = 0,
    fn send(ctx: ?*anyopaque, request: sdk.HttpRequest, allocator: Allocator) anyerror!sdk.HttpResponse {
        const self: *Route = @ptrCast(@alignCast(ctx.?));
        const expected = try std.fmt.allocPrint(allocator, "http://offline.invalid/twirp/livekit.{s}", .{self.expected});
        try std.testing.expectEqualStrings(expected, request.url);
        try std.testing.expectEqualStrings("POST", request.method);
        try std.testing.expectEqual(@as(i64, 1234), request.timeout_ms);
        var protobuf = false;
        var auth = false;
        for (request.headers) |h| {
            if (std.mem.eql(u8, h[0], "Content-Type")) protobuf = std.mem.eql(u8, h[1], "application/protobuf");
            if (std.mem.eql(u8, h[0], "Authorization")) auth = std.mem.startsWith(u8, h[1], "Bearer ");
        }
        try std.testing.expect(protobuf and auth);
        self.calls += 1;
        return .{ .status_code = 200 };
    }
};
test "all 52 service routes and typed JSON responses" {
    const n = try library();
    defer n.release();
    var calls: usize = 0;
    inline for (@import("routes.zig").routes) |route| {
        var ctx = Route{ .expected = route[3] };
        var api = try client(n, .{ .context = &ctx, .send = Route.send });
        defer api.deinit();
        var request = try route[2].empty(a);
        defer request.deinit();
        const service = @field(sdk.LiveKitApi, route[0])(&api);
        var result = try @field(@TypeOf(service), route[1])(service, a, request);
        defer result.deinit();
        try std.testing.expect(result == .value);
        try std.testing.expectEqualStrings("{}", result.value.json);
        calls += ctx.calls;
    }
    try std.testing.expectEqual(@as(usize, 52), calls);
}

const Binary = struct {
    const name = "中文😀\x00";
    fn send(_: ?*anyopaque, request: sdk.HttpRequest, allocator: Allocator) anyerror!sdk.HttpResponse {
        const expected = try std.mem.concat(allocator, u8, &.{ &.{ 10, name.len }, name });
        try std.testing.expectEqualSlices(u8, expected, request.body);
        return .{ .status_code = 200, .body = try std.mem.concat(allocator, u8, &.{ &.{ 18, name.len }, name }) };
    }
    fn found(_: ?*anyopaque, request: sdk.HttpRequest, _: Allocator) anyerror!sdk.HttpResponse {
        for (request.headers) |h| if (std.mem.eql(u8, h[0], "Authorization")) {
            try std.testing.expectEqualStrings("Bearer preissued-test-token", h[1]);
        };
        return .{ .status_code = 200, .body = "\x0a\x06\x0a\x04AD_1" };
    }
};
test "binary Unicode NUL preissued token and optional dispatch" {
    const n = try library();
    defer n.release();
    var api = try client(n, .{ .send = Binary.send });
    defer api.deinit();
    var request = try sdk.models.CreateRoomRequest.fromValue(a, .{ .name = Binary.name });
    defer request.deinit();
    var result = try api.room().createRoom(a, request);
    defer result.deinit();
    try std.testing.expect(result == .value);
    var room = try result.value.toValue(a);
    defer room.deinit();
    try std.testing.expectEqualStrings(Binary.name, try text(room.value, "name"));
    var found_api = try sdk.LiveKitApi.init(a, n, .{ .url = "http://offline.invalid", .access_token = "preissued-test-token", .transport = .{ .send = Binary.found } });
    defer found_api.deinit();
    var found = try found_api.agentDispatch().getDispatch(a, "AD_1", "room");
    defer found.deinit();
    try std.testing.expect(found == .value and found.value != null);
    var dispatch = try found.value.?.toValue(a);
    defer dispatch.deinit();
    try std.testing.expectEqualStrings("AD_1", try text(dispatch.value, "id"));
    var empty_api = try client(n, .{ .send = empty });
    defer empty_api.deinit();
    var missing = try empty_api.agentDispatch().getDispatch(a, "missing", "room");
    defer missing.deinit();
    try std.testing.expect(missing == .value and missing.value == null);
}

test "independent token HMAC clock grants and input snapshots" {
    const n = try library();
    defer n.release();
    var token = try sdk.AccessToken.init(a, n, key, secret);
    defer token.deinit();
    var identity = [_]u8{ 'a', 'l', 'i', 'c', 'e', 0 };
    try token.setIdentity(&identity);
    identity[0] = 'X';
    try token.setName("中文😀");
    try token.setMetadata("quote\"\n\x00");
    try token.setValidFor(60);
    try token.setAttributes(&.{.{ .key = "语言", .value = "中文😀" }});
    var sources = [_][]const u8{"microphone"};
    try token.setVideoGrant(.{ .room_join = true, .room = "room", .can_publish = false, .can_publish_sources = &sources });
    sources[0] = "camera";
    try token.setSipGrant(.{ .call = true });
    try token.setAgentGrant(.{ .simulation_admin = true });
    var jwt = try token.toJwtAt(a, 1700000000);
    defer jwt.deinit();
    try std.testing.expect(jwt == .value);
    try expectSignature(jwt.value.bytes);
    var parsed = try claims(jwt.value.bytes);
    defer parsed.deinit();
    const c = parsed.value;
    try std.testing.expectEqualStrings("alice\x00", try text(c, "sub"));
    try std.testing.expectEqualStrings("中文😀", try text(c, "name"));
    try std.testing.expectEqualStrings("quote\"\n\x00", try text(c, "metadata"));
    try std.testing.expectEqualStrings("中文😀", try text(try field(c, "attributes"), "语言"));
    const video = try field(c, "video");
    try std.testing.expect(!(try field(video, "canPublish")).bool);
    try std.testing.expect(!video.object.contains("canSubscribe"));
    try std.testing.expectEqualStrings("microphone", (try field(video, "canPublishSources")).array.items[0].string);
    try std.testing.expect((try field(try field(c, "sip"), "call")).bool);
    try std.testing.expect((try field(try field(c, "agent"), "simulationAdmin")).bool);
    try std.testing.expectEqual(@as(i64, 1700000000), (try field(c, "nbf")).integer);
    try std.testing.expectEqual(@as(i64, 1700000060), (try field(c, "exp")).integer);
    try std.testing.expectError(error.InvalidLifetime, token.setValidFor(0));
    var negative = try token.toJwtAt(a, -1);
    defer negative.deinit();
    try expectCategory(negative, sdk.SdkError.invalid_argument);
    try token.setValidFor(std.math.maxInt(i64));
    var overflow = try token.toJwtAt(a, 1700000000);
    defer overflow.deinit();
    try expectCategory(overflow, sdk.SdkError.invalid_argument);
    var current_token = try sdk.AccessToken.init(a, n, key, secret);
    defer current_token.deinit();
    var current = try current_token.toJwt(a, std.testing.io);
    defer current.deinit();
    try std.testing.expect(current == .value);
    try expectSignature(current.value.bytes);
}

const Errors = struct {
    fn protocol(_: ?*anyopaque, _: sdk.HttpRequest, _: Allocator) anyerror!sdk.HttpResponse {
        return .{ .status_code = 200, .body = "\xff" };
    }
    fn status(_: ?*anyopaque, _: sdk.HttpRequest, _: Allocator) anyerror!sdk.HttpResponse {
        return .{ .status_code = 999 };
    }
    fn http(_: ?*anyopaque, _: sdk.HttpRequest, _: Allocator) anyerror!sdk.HttpResponse {
        return .{ .status_code = 403, .body = "{\"code\":\"permission_denied\",\"msg\":\"denied 中文😀\"}" };
    }
    fn host(_: ?*anyopaque, _: sdk.HttpRequest, _: Allocator) anyerror!sdk.HttpResponse {
        return error.HostTransportSentinel;
    }
};
test "SDK category HTTP Twirp details and original host errors" {
    const n = try library();
    defer n.release();
    try std.testing.expectError(error.InvalidTimeout, sdk.LiveKitApi.init(a, n, .{ .timeout_ms = 0 }));
    var api = try client(n, .{ .send = empty });
    defer api.deinit();
    for ([_][]const u8{ "not JSON", "{\"unknown\":true}" }) |json| {
        var request = try sdk.models.CreateRoomRequest.initJson(a, json);
        defer request.deinit();
        var result = try api.room().createRoom(a, request);
        defer result.deinit();
        try expectCategory(result, sdk.SdkError.protocol);
    }
    inline for (.{ .{ Errors.protocol, sdk.SdkError.protocol }, .{ Errors.status, sdk.SdkError.transport }, .{ Errors.http, sdk.SdkError.http } }) |pair| {
        var failing = try client(n, .{ .send = pair[0] });
        defer failing.deinit();
        var request = try sdk.models.ListRoomsRequest.empty(a);
        defer request.deinit();
        var result = try failing.room().listRooms(a, request);
        defer result.deinit();
        try expectCategory(result, pair[1]);
        if (pair[1] == sdk.SdkError.http) {
            try std.testing.expectEqual(@as(i32, 403), result.failure.http_status);
            try std.testing.expectEqualStrings("permission_denied", result.failure.twirp_code);
            try std.testing.expectEqualStrings("denied 中文😀", result.failure.message);
        }
    }
    var failing = try client(n, .{ .send = Errors.host });
    defer failing.deinit();
    var request = try sdk.models.ListRoomsRequest.empty(a);
    defer request.deinit();
    try std.testing.expectError(error.HostTransportSentinel, failing.room().listRooms(a, request));
    var empty_credentials = try sdk.AccessToken.init(a, n, "", "");
    defer empty_credentials.deinit();
    var authentication = try empty_credentials.toJwtAt(a, 1700000000);
    defer authentication.deinit();
    try expectCategory(authentication, sdk.SdkError.authentication);
}

const body = "{\"event\":\"participant_joined\",\"id\":\"EV_1\",\"createdAt\":\"1700000000\",\"room\":{\"sid\":\"RM_1\",\"name\":\"room\",\"metadata\":\"r\"},\"participant\":{\"sid\":\"PA_1\",\"identity\":\"alice\",\"name\":\"A\",\"metadata\":\"p\"},\"track\":{\"sid\":\"TR_1\",\"name\":\"microphone\"},\"egressInfo\":{\"egressId\":\"EG_1\",\"roomId\":\"RM_1\",\"roomName\":\"room\"},\"ingressInfo\":{\"ingressId\":\"IN_1\",\"name\":\"input\",\"roomName\":\"room\"},\"futureField\":{\"中文😀\":true}}";
const Claims = struct { issuer: []const u8 = key, nbf: ?i64 = null, exp: ?i64 = null };
fn authorization(allocator: Allocator, changes: Claims) ![]const u8 {
    var arena = std.heap.ArenaAllocator.init(allocator);
    defer arena.deinit();
    const t = arena.allocator();
    const now = std.Io.Clock.real.now(std.testing.io).toSeconds();
    var checksum: [32]u8 = undefined;
    std.crypto.hash.sha2.Sha256.hash(body, &checksum, .{});
    const encoder = std.base64.standard.Encoder;
    const checksum64 = encoder.encode(try t.alloc(u8, encoder.calcSize(checksum.len)), &checksum);
    const json = try std.json.Stringify.valueAlloc(t, .{ .iss = changes.issuer, .nbf = changes.nbf orelse now - 60, .exp = changes.exp orelse now + 3600, .sha256 = checksum64 }, .{});
    const unsigned = try std.fmt.allocPrint(t, "{s}.{s}", .{ try b64(t, "{\"alg\":\"HS256\",\"typ\":\"JWT\"}"), try b64(t, json) });
    return std.fmt.allocPrint(allocator, "{s}.{s}", .{ unsigned, try b64(t, &sign(unsigned)) });
}
const Delivery = struct {
    count: usize = 0,
    fn receive(ctx: ?*anyopaque, event: *const sdk.WebhookSnapshot) anyerror!void {
        const self: *Delivery = @ptrCast(@alignCast(ctx.?));
        try std.testing.expectEqualStrings("EV_1", event.id);
        self.count += 1;
    }
};
test "webhook raw snapshots multiple secrets tampering lifetime and signature" {
    const n = try library();
    defer n.release();
    var receiver = try sdk.WebhookReceiver.initSecrets(a, n, &.{ .{ .key = "other", .secret = "unused" }, .{ .key = key, .secret = secret } });
    defer receiver.deinit();
    var delivered = Delivery{};
    receiver.setCallback(.{ .context = &delivered, .deliver = Delivery.receive });
    const auth = try authorization(a, .{});
    defer a.free(auth);
    const bearer = try std.fmt.allocPrint(a, "Bearer {s}", .{auth});
    defer a.free(bearer);
    var result = try receiver.receiveAndDispatch(a, body, bearer);
    defer result.deinit();
    try std.testing.expect(result == .value);
    const event = result.value.snapshot;
    try std.testing.expectEqualStrings(body, event.raw_body);
    try std.testing.expectEqual(@as(i64, 1700000000), event.created_at);
    try std.testing.expectEqualStrings("RM_1", event.room.?.sid);
    try std.testing.expectEqualStrings("alice", event.participant.?.identity);
    try std.testing.expectEqualStrings("TR_1", event.track.?.sid);
    try std.testing.expectEqualStrings("EG_1", event.egress.?.egress_id);
    try std.testing.expectEqualStrings("IN_1", event.ingress.?.ingress_id);
    var tampered = try receiver.receiveAndDispatch(a, body ++ " ", auth);
    defer tampered.deinit();
    try expectCategory(tampered, sdk.SdkError.authentication);
    const now = std.Io.Clock.real.now(std.testing.io).toSeconds();
    for ([_]Claims{ .{ .issuer = "unknown" }, .{ .nbf = now - 7200, .exp = now - 3600 }, .{ .nbf = now + 3600, .exp = now + 7200 } }) |change| {
        const invalid = try authorization(a, change);
        defer a.free(invalid);
        var rejected = try receiver.receiveAndDispatch(a, body, invalid);
        defer rejected.deinit();
        try expectCategory(rejected, sdk.SdkError.authentication);
    }
    const dot = std.mem.lastIndexOfScalar(u8, auth, '.').?;
    const zero_bytes: [32]u8 = @splat(0);
    const zero = try b64(a, &zero_bytes);
    defer a.free(zero);
    const invalid = try std.fmt.allocPrint(a, "{s}.{s}", .{ auth[0..dot], zero });
    defer a.free(invalid);
    var rejected = try receiver.receiveAndDispatch(a, body, invalid);
    defer rejected.deinit();
    try expectCategory(rejected, sdk.SdkError.authentication);
    try std.testing.expectEqual(@as(usize, 1), delivered.count);
}

const Keys = struct {
    inner: *const sdk.LiveKitApi,
    count: usize = 0,
    fn lookup(ctx: ?*anyopaque, issuer: []const u8, allocator: Allocator) anyerror!?[]const u8 {
        const self: *Keys = @ptrCast(@alignCast(ctx.?));
        self.count += 1;
        var request = try sdk.models.ListRoomsRequest.empty(allocator);
        defer request.deinit();
        var result = try self.inner.room().listRooms(allocator, request);
        defer result.deinit();
        try std.testing.expect(result == .value);
        return if (std.mem.eql(u8, issuer, key)) secret else null;
    }
    fn fail(_: ?*anyopaque, _: []const u8, _: Allocator) anyerror!?[]const u8 {
        return error.KeyProviderSentinel;
    }
};
const Replace = struct {
    receiver: *sdk.WebhookReceiver,
    count: usize = 0,
    fn first(ctx: ?*anyopaque, event: *const sdk.WebhookSnapshot) anyerror!void {
        const self: *Replace = @ptrCast(@alignCast(ctx.?));
        self.receiver.setCallback(.{ .context = self, .deliver = second });
        try self.receiver.dispatch(event);
    }
    fn second(ctx: ?*anyopaque, _: *const sdk.WebhookSnapshot) anyerror!void {
        const self: *Replace = @ptrCast(@alignCast(ctx.?));
        self.count += 1;
    }
    fn fail(_: ?*anyopaque, _: *const sdk.WebhookSnapshot) anyerror!void {
        return error.DeliverySentinel;
    }
};
test "nested dynamic key lookup original errors and reentrant event replacement" {
    const n = try library();
    defer n.release();
    var inner = try client(n, .{ .send = empty });
    defer inner.deinit();
    var keys = Keys{ .inner = &inner };
    var receiver = try sdk.WebhookReceiver.initKeyProvider(a, n, .{ .context = &keys, .lookup = Keys.lookup });
    defer receiver.deinit();
    const auth = try authorization(a, .{});
    defer a.free(auth);
    var event = try receiver.receive(a, body, auth);
    defer event.deinit();
    try std.testing.expect(event == .value);
    try std.testing.expectEqual(@as(usize, 1), keys.count);
    const unknown = try authorization(a, .{ .issuer = "unknown" });
    defer a.free(unknown);
    var rejected = try receiver.receive(a, body, unknown);
    defer rejected.deinit();
    try expectCategory(rejected, sdk.SdkError.authentication);
    var failed = try sdk.WebhookReceiver.initKeyProvider(a, n, .{ .lookup = Keys.fail });
    defer failed.deinit();
    try std.testing.expectError(error.KeyProviderSentinel, failed.receive(a, body, auth));
    var replacement = Replace{ .receiver = &receiver };
    receiver.setCallback(.{ .context = &replacement, .deliver = Replace.first });
    try receiver.dispatch(&event.value.snapshot);
    try std.testing.expectEqual(@as(usize, 1), replacement.count);
    receiver.setCallback(null);
    try receiver.dispatch(&event.value.snapshot);
    try std.testing.expectEqual(@as(usize, 1), replacement.count);
    receiver.setCallback(.{ .deliver = Replace.fail });
    try std.testing.expectError(error.DeliverySentinel, receiver.receiveAndDispatch(a, body, auth));
}

threadlocal var invoking_thread: bool = false;
const Concurrent = struct {
    inner: *const sdk.LiveKitApi,
    token: *const sdk.AccessToken,
    calls: std.atomic.Value(usize) = .init(0),
    fn send(ctx: ?*anyopaque, _: sdk.HttpRequest, allocator: Allocator) anyerror!sdk.HttpResponse {
        const self: *Concurrent = @ptrCast(@alignCast(ctx.?));
        try std.testing.expect(invoking_thread);
        var request = try sdk.models.ListRoomsRequest.empty(allocator);
        defer request.deinit();
        var nested = try self.inner.room().listRooms(allocator, request);
        defer nested.deinit();
        try std.testing.expect(nested == .value);
        var jwt = try self.token.toJwtAt(allocator, 1700000000);
        defer jwt.deinit();
        try std.testing.expect(jwt == .value);
        _ = self.calls.fetchAdd(1, .monotonic);
        return .{ .status_code = 200, .body = "\x12\x04room" };
    }
    const Worker = struct {
        api: *const sdk.LiveKitApi,
        failure: ?anyerror = null,
        fn run(self: *Worker) void {
            self.work() catch |err| {
                self.failure = err;
            };
        }
        fn work(self: *Worker) !void {
            invoking_thread = true;
            defer invoking_thread = false;
            for (0..10) |_| {
                var request = try sdk.models.CreateRoomRequest.empty(a);
                defer request.deinit();
                var result = try self.api.room().createRoom(a, request);
                defer result.deinit();
                try std.testing.expect(result == .value);
                var room = try result.value.toValue(a);
                defer room.deinit();
                try std.testing.expectEqualStrings("room", try text(room.value, "name"));
            }
        }
    };
};
test "eight concurrent threads with nested transport and signing" {
    const n = try library();
    defer n.release();
    var inner = try client(n, .{ .send = empty });
    defer inner.deinit();
    var token = try sdk.AccessToken.init(a, n, key, secret);
    defer token.deinit();
    var ctx = Concurrent{ .inner = &inner, .token = &token };
    var api = try client(n, .{ .context = &ctx, .send = Concurrent.send });
    defer api.deinit();
    var workers: [8]Concurrent.Worker = undefined;
    var threads: [8]std.Thread = undefined;
    var started: usize = 0;
    errdefer for (threads[0..started]) |thread| thread.join();
    for (&workers, &threads) |*worker, *thread| {
        worker.* = .{ .api = &api };
        thread.* = try std.Thread.spawn(.{}, Concurrent.Worker.run, .{worker});
        started += 1;
    }
    for (threads) |thread| thread.join();
    started = 0;
    for (workers) |worker| if (worker.failure) |err| return err;
    try std.testing.expectEqual(@as(usize, 80), ctx.calls.load(.acquire));
}
fn retainedApi() !sdk.LiveKitApi {
    const n = try library();
    defer n.release();
    return client(n, .{ .send = empty });
}
test "API retains module after original reference release" {
    var api = try retainedApi();
    defer api.deinit();
    var request = try sdk.models.ListRoomsRequest.empty(a);
    defer request.deinit();
    var result = try api.room().listRooms(a, request);
    defer result.deinit();
    try std.testing.expect(result == .value);
}
