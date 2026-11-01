const std = @import("std");
const builtin = @import("builtin");
const types = @import("result.zig");
const Allocator = std.mem.Allocator;

pub const Callback = *const fn (?*anyopaque, [*]const u8, usize, *?[*]const u8, *usize) callconv(.c) i32;
pub const Invoke = *const fn ([*]const u8, usize, ?Callback, ?*anyopaque, *usize) callconv(.c) ?*anyopaque;
pub const Free = *const fn (?*anyopaque) callconv(.c) void;

const WindowsLoader = struct {
    extern "kernel32" fn LoadLibraryW([*:0]const u16) callconv(.winapi) ?*anyopaque;
    extern "kernel32" fn GetProcAddress(*anyopaque, [*:0]const u8) callconv(.winapi) ?*anyopaque;
    extern "kernel32" fn FreeLibrary(*anyopaque) callconv(.winapi) i32;
    handle: *anyopaque,

    fn open(allocator: Allocator, path: []const u8) !WindowsLoader {
        const wide = try std.unicode.utf8ToUtf16LeAllocZ(allocator, path);
        defer allocator.free(wide);
        return .{ .handle = LoadLibraryW(wide) orelse return error.NativeLibraryNotFound };
    }
    fn lookup(self: *WindowsLoader, comptime T: type, name: [:0]const u8) ?T {
        return @ptrCast(GetProcAddress(self.handle, name) orelse return null);
    }
    fn close(self: *WindowsLoader) void {
        _ = FreeLibrary(self.handle);
    }
};
const Loader = if (builtin.os.tag == .windows) WindowsLoader else std.DynLib;

pub const HttpRequest = struct {
    url: []const u8,
    method: []const u8,
    headers: []const [2][]const u8,
    body: []const u8,
    timeout_ms: i64,
};
pub const HttpResponse = struct { status_code: i32, body: []const u8 = "" };

/// Context is application-owned. Allocate temporary response bytes with the provided call arena;
/// never return a slice into the callback's stack. All callbacks run synchronously on this thread.
pub const Transport = struct {
    context: ?*anyopaque = null,
    send: *const fn (?*anyopaque, HttpRequest, Allocator) anyerror!HttpResponse,
};
pub const KeyProvider = struct {
    context: ?*anyopaque = null,
    lookup: *const fn (?*anyopaque, []const u8, Allocator) anyerror!?[]const u8,
};

/// Reference-counted module. Own one reference from open(), release() it once. Clients retain it.
/// Load only trusted SDK code and dependencies satisfying the full ABI 1 lifetime/threading contract.
pub const NativeLibrary = struct {
    allocator: Allocator,
    refs: std.atomic.Value(usize) = .init(1),
    loader: ?Loader,
    invoke_fn: Invoke,
    free_fn: Free,

    pub const default_filename = switch (builtin.os.tag) {
        .windows => "livekitserver.dll",
        .macos => "liblivekitserver.dylib",
        else => "liblivekitserver.so",
    };

    pub fn open(allocator: Allocator, path: []const u8) !*NativeLibrary {
        if (std.mem.indexOfScalar(u8, path, 0) != null) return error.InvalidLibraryPath;
        var loader = if (builtin.os.tag == .windows) try Loader.open(allocator, path) else try Loader.open(path);
        errdefer loader.close();
        const version = loader.lookup(*const fn () callconv(.c) i32, "lks_abi_version") orelse return error.MissingAbiExport;
        if (version() != 1) return error.UnsupportedAbiVersion;
        const invoke_fn = loader.lookup(Invoke, "lks_invoke") orelse return error.MissingAbiExport;
        const free_fn = loader.lookup(Free, "lks_free") orelse return error.MissingAbiExport;
        const self = try allocator.create(NativeLibrary);
        self.* = .{ .allocator = allocator, .loader = loader, .invoke_fn = invoke_fn, .free_fn = free_fn };
        return self;
    }
    pub fn retain(self: *NativeLibrary) *NativeLibrary {
        _ = self.refs.fetchAdd(1, .monotonic);
        return self;
    }
    pub fn release(self: *NativeLibrary) void {
        if (self.refs.fetchSub(1, .acq_rel) == 1) {
            if (self.loader) |*loader| loader.close();
            self.allocator.destroy(self);
        }
    }

    pub fn invoke(self: *NativeLibrary, allocator: Allocator, input: anytype, transport: ?Transport, key: ?KeyProvider) !Envelope {
        // The active call holds a lease even if a reentrant callback drops another owner.
        _ = self.retain();
        defer self.release();
        var arena = std.heap.ArenaAllocator.init(allocator);
        defer arena.deinit();
        const temporary = arena.allocator();
        const json = try std.json.Stringify.valueAlloc(temporary, input, .{ .emit_null_optional_fields = false });
        var context = Context{ .allocator = temporary, .transport = transport, .key = key };
        var size: usize = 0;
        const pointer = self.invoke_fn(json.ptr, json.len, if (transport != null or key != null) exchange else null, &context, &size);
        // Always use the allocating native SDK, including native errors, decode failures and host errors.
        defer if (pointer) |p| self.free_fn(p);
        if (context.failure) |err| return err;
        const output: [*]const u8 = @ptrCast(pointer orelse return error.NativeAllocationFailure);
        var parsed = try std.json.parseFromSlice(std.json.Value, allocator, output[0..size], .{ .allocate = .alloc_always });
        errdefer parsed.deinit();
        if (parsed.value != .object) return error.InvalidNativeEnvelope;
        const ok = parsed.value.object.get("ok") orelse return error.InvalidNativeEnvelope;
        if (ok != .bool) return error.InvalidNativeEnvelope;
        if (ok.bool and !parsed.value.object.contains("value")) return error.InvalidNativeEnvelope;
        return .{ .parsed = parsed };
    }
};

pub const Envelope = struct {
    parsed: std.json.Parsed(std.json.Value),
    pub fn deinit(self: *Envelope) void {
        self.parsed.deinit();
        self.* = undefined;
    }
    pub fn isOk(self: Envelope) bool {
        return self.parsed.value.object.get("ok").?.bool;
    }
    pub fn value(self: Envelope) std.json.Value {
        return self.parsed.value.object.get("value").?;
    }
    pub fn into(self: Envelope, comptime T: type, allocator: Allocator) !types.Result(T) {
        if (!self.isOk()) return .{ .failure = try types.SdkError.fromValue(allocator, self.parsed.value) };
        const v = self.value();
        if (v != .string) return error.ExpectedNativeString;
        return .{ .value = try T.initJson(allocator, v.string) };
    }
};

const Context = struct {
    allocator: Allocator,
    transport: ?Transport,
    key: ?KeyProvider,
    failure: ?anyerror = null,

    fn handle(self: *Context, bytes: []const u8) ![]const u8 {
        const request = try std.json.parseFromSlice(std.json.Value, self.allocator, bytes, .{});
        const kind = try stringField(request.value, "kind");
        if (std.mem.eql(u8, kind, "transport")) {
            const transport = self.transport orelse return error.MissingTransport;
            const Wire = struct { kind: []const u8, url: []const u8, method: []const u8, headers: []const [2][]const u8, body: []const u8, timeout_ms: []const u8 };
            const wire = try std.json.parseFromValue(Wire, self.allocator, request.value, .{});
            const result = try transport.send(transport.context, .{ .url = wire.value.url, .method = wire.value.method, .headers = wire.value.headers, .body = try unbase64(self.allocator, wire.value.body), .timeout_ms = try std.fmt.parseInt(i64, wire.value.timeout_ms, 10) }, self.allocator);
            return std.json.Stringify.valueAlloc(self.allocator, .{
                .status_code = try std.fmt.allocPrint(self.allocator, "{d}", .{result.status_code}),
                .body = try base64(self.allocator, result.body),
            }, .{});
        } else if (std.mem.eql(u8, kind, "key")) {
            const key = self.key orelse return error.MissingKeyProvider;
            const secret = try key.lookup(key.context, try stringField(request.value, "key"), self.allocator);
            return std.json.Stringify.valueAlloc(self.allocator, .{ .secret = secret orelse "" }, .{});
        }
        return error.UnknownCallbackKind;
    }
};

fn exchange(user: ?*anyopaque, request: [*]const u8, length: usize, response: *?[*]const u8, response_length: *usize) callconv(.c) i32 {
    const context: *Context = @ptrCast(@alignCast(user.?));
    if (context.failure != null) return 1;
    const bytes = context.handle(request[0..length]) catch |err| {
        context.failure = err;
        return 1;
    };
    response.* = bytes.ptr;
    response_length.* = bytes.len;
    return 0;
}

pub fn stringField(value: std.json.Value, field: []const u8) ![]const u8 {
    if (value != .object) return error.ExpectedJsonObject;
    const result = value.object.get(field) orelse return error.MissingJsonField;
    if (result != .string) return error.ExpectedJsonString;
    return result.string;
}
pub fn base64(allocator: Allocator, bytes: []const u8) ![]const u8 {
    const encoder = std.base64.url_safe_no_pad.Encoder;
    const output = try allocator.alloc(u8, encoder.calcSize(bytes.len));
    return encoder.encode(output, bytes);
}
pub fn unbase64(allocator: Allocator, text: []const u8) ![]const u8 {
    const decoder = std.base64.url_safe_no_pad.Decoder;
    const output = try allocator.alloc(u8, try decoder.calcSizeForSlice(text));
    errdefer allocator.free(output);
    try decoder.decode(output, text);
    return output;
}

const Fixture = struct {
    var frees: usize = 0;

    fn free(pointer: ?*anyopaque) callconv(.c) void {
        const bytes: [*:0]const u8 = @ptrCast(pointer.?);
        std.testing.allocator.free(std.mem.span(bytes));
        frees += 1;
    }
    fn invoke(input: [*]const u8, length: usize, callback: ?Callback, user: ?*anyopaque, out_len: *usize) callconv(.c) ?*anyopaque {
        const a = std.testing.allocator;
        const parsed = std.json.parseFromSlice(std.json.Value, a, input[0..length], .{}) catch @panic("bad fixture input");
        defer parsed.deinit();
        const mode = stringField(parsed.value, "operation") catch @panic("missing fixture operation");
        const output: []const u8 = if (std.mem.eql(u8, mode, "null")) blk: {
            out_len.* = 0;
            break :blk "";
        } else if (std.mem.eql(u8, mode, "bad")) "\xff" else if (std.mem.eql(u8, mode, "failure"))
            "{\"ok\":false,\"code\":123,\"message\":\"future\",\"http_status\":403,\"twirp_code\":\"denied\"}"
        else if (std.mem.eql(u8, mode, "callback") or std.mem.eql(u8, mode, "key")) blk: {
            const request = if (std.mem.eql(u8, mode, "key")) "{\"kind\":\"key\",\"key\":\"issuer\"}" else "{\"kind\":\"transport\",\"url\":\"https://offline.invalid\",\"method\":\"POST\",\"headers\":[],\"body\":\"AAE\",\"timeout_ms\":\"1\"}";
            var first: ?[*]const u8 = null;
            var first_len: usize = 0;
            const status = callback.?(user, request.ptr, request.len, &first, &first_len);
            if (status == 0) {
                var second: ?[*]const u8 = null;
                var second_len: usize = 0;
                if (callback.?(user, request.ptr, request.len, &second, &second_len) != 0) break :blk "{\"ok\":true,\"value\":\"{}\"}";
                // Both callback allocations must still be readable before invoke returns.
                std.debug.assert(std.mem.eql(u8, first.?[0..first_len], second.?[0..second_len]));
            }
            break :blk "{\"ok\":true,\"value\":\"{}\"}";
        } else if (std.mem.eql(u8, mode, "webhook"))
            "{\"ok\":true,\"value\":{\"event\":\"room_started\",\"id\":\"EV_mock\",\"created_at\":\"1\",\"raw_body\":\"{}\"}}"
        else if (std.mem.eql(u8, mode, "get_dispatch")) "{\"ok\":true,\"value\":null}" else "{\"ok\":true,\"value\":\"{}\"}";
        if (output.len == 0) return null;
        const copy = a.dupeSentinel(u8, output, 0) catch return null;
        out_len.* = copy.len;
        return copy.ptr;
    }
    fn library(allocator: Allocator) !*NativeLibrary {
        const n = try allocator.create(NativeLibrary);
        n.* = .{ .allocator = allocator, .loader = null, .invoke_fn = invoke, .free_fn = free };
        return n;
    }
    fn send(_: ?*anyopaque, request: HttpRequest, allocator: Allocator) anyerror!HttpResponse {
        try std.testing.expectEqualSlices(u8, &.{ 0, 1 }, request.body);
        return .{ .status_code = 200, .body = try allocator.dupe(u8, request.body) };
    }
    fn fail(_: ?*anyopaque, _: HttpRequest, _: Allocator) anyerror!HttpResponse {
        return error.HostSentinel;
    }
    fn lookup(_: ?*anyopaque, _: []const u8, allocator: Allocator) anyerror!?[]const u8 {
        return try allocator.dupe(u8, "temporary-secret");
    }
};

test "native results freed once on success failure invalid JSON null and host error" {
    const a = std.testing.allocator;
    const n = try Fixture.library(a);
    defer n.release();
    Fixture.frees = 0;
    {
        var success = try n.invoke(a, .{ .operation = "success" }, null, null);
        defer success.deinit();
        try std.testing.expectEqual(@as(usize, 1), Fixture.frees);
        try std.testing.expectEqualStrings("{}", success.value().string);
    }
    {
        var envelope = try n.invoke(a, .{ .operation = "failure" }, null, null);
        defer envelope.deinit();
        var result = try envelope.into(types.OwnedString, a);
        defer result.deinit();
        try std.testing.expect(result == .failure);
        try std.testing.expectEqual(@as(i32, 123), result.failure.code);
        try std.testing.expectEqualStrings("denied", result.failure.twirp_code);
    }
    try std.testing.expectEqual(@as(usize, 2), Fixture.frees);
    if (n.invoke(a, .{ .operation = "bad" }, null, null)) |value| {
        var unexpected = value;
        unexpected.deinit();
        return error.ExpectedDecodeFailure;
    } else |_| {}
    try std.testing.expectEqual(@as(usize, 3), Fixture.frees);
    try std.testing.expectError(error.NativeAllocationFailure, n.invoke(a, .{ .operation = "null" }, null, null));
    try std.testing.expectEqual(@as(usize, 3), Fixture.frees);
    {
        var callback = try n.invoke(a, .{ .operation = "callback" }, .{ .send = Fixture.send }, null);
        defer callback.deinit();
    }
    try std.testing.expectEqual(@as(usize, 4), Fixture.frees);
    try std.testing.expectError(error.HostSentinel, n.invoke(a, .{ .operation = "callback" }, .{ .send = Fixture.fail }, null));
    try std.testing.expectEqual(@as(usize, 5), Fixture.frees);
}

fn allocationScenario(a: Allocator) !void {
    const n = try Fixture.library(a);
    defer n.release();
    var callback = try n.invoke(a, .{ .operation = "callback" }, .{ .send = Fixture.send }, null);
    defer callback.deinit();
    var key_result = try n.invoke(a, .{ .operation = "key" }, null, .{ .lookup = Fixture.lookup });
    defer key_result.deinit();
    var api = try @import("api.zig").LiveKitApi.init(a, n, .{ .api_key = "test", .api_secret = "secret" });
    defer api.deinit();
    var request = try @import("models.zig").CreateRoomRequest.fromValue(a, .{ .name = "room\x00中文" });
    defer request.deinit();
    var result = try api.room().createRoom(a, request);
    defer result.deinit();
    var missing = try api.agentDispatch().getDispatch(a, "missing", "room");
    defer missing.deinit();
    var token = try @import("token.zig").AccessToken.init(a, n, "key", "secret");
    defer token.deinit();
    try token.setIdentity("alice");
    try token.setName("A");
    try token.setMetadata("quote\"\x00");
    try token.setAttributes(&.{.{ .key = "语言", .value = "中文" }});
    try token.setVideoGrant(.{ .room_join = true, .room = "room", .can_publish = false, .can_publish_sources = &.{"microphone"} });
    try token.setSipGrant(.{ .call = true });
    try token.setAgentGrant(.{ .simulation_admin = true });
    try token.setValidFor(60);
    var jwt = try token.toJwtAt(a, 1700000000);
    defer jwt.deinit();
    var receiver = try @import("webhook.zig").WebhookReceiver.init(a, n, "key", "secret");
    defer receiver.deinit();
    var event = try receiver.receiveAndDispatch(a, "{}", "signature");
    defer event.deinit();
    var failure = try n.invoke(a, .{ .operation = "failure" }, null, null);
    defer failure.deinit();
    var native_error = try failure.into(types.OwnedString, a);
    defer native_error.deinit();
}

test "all allocation failure paths clean up owning results builders and webhook snapshots" {
    try std.testing.checkAllAllocationFailures(std.testing.allocator, allocationScenario, .{});
}
