const std = @import("std");
const native_mod = @import("native.zig");
const Allocator = std.mem.Allocator;
const NativeLibrary = native_mod.NativeLibrary;
const types = @import("result.zig");

pub const WebhookRoom = struct { sid: []const u8, name: []const u8, metadata: []const u8 };
pub const WebhookParticipant = struct { sid: []const u8, identity: []const u8, name: []const u8, metadata: []const u8 };
pub const WebhookTrack = struct { sid: []const u8, name: []const u8 };
pub const WebhookEgress = struct { egress_id: []const u8, room_id: []const u8, room_name: []const u8 };
pub const WebhookIngress = struct { ingress_id: []const u8, name: []const u8, room_name: []const u8 };

/// All slices are borrowed from the owning WebhookEvent, which retains the complete verified body.
pub const WebhookSnapshot = struct {
    event: []const u8,
    id: []const u8,
    created_at: i64,
    raw_body: []const u8,
    room: ?WebhookRoom,
    participant: ?WebhookParticipant,
    track: ?WebhookTrack,
    egress: ?WebhookEgress,
    ingress: ?WebhookIngress,
};

pub const WebhookEvent = struct {
    owner: std.json.Parsed(std.json.Value),
    snapshot: WebhookSnapshot,

    pub fn deinit(self: *WebhookEvent) void {
        self.owner.deinit();
        self.* = undefined;
    }
};

pub const WebhookCallback = struct {
    context: ?*anyopaque = null,
    deliver: *const fn (?*anyopaque, *const WebhookSnapshot) anyerror!void,
};

pub const WebhookReceiver = struct {
    native: *NativeLibrary,
    arena: std.heap.ArenaAllocator,
    secrets: []const Secret,
    key_provider: ?native_mod.KeyProvider = null,
    callback: ?WebhookCallback = null,
    callback_lock: std.atomic.Mutex = .unlocked,

    pub const Secret = struct { key: []const u8, secret: []const u8 };

    pub fn init(allocator: Allocator, native: *NativeLibrary, key: []const u8, secret: []const u8) !WebhookReceiver {
        return initSecrets(allocator, native, &.{.{ .key = key, .secret = secret }});
    }
    pub fn initSecrets(allocator: Allocator, native: *NativeLibrary, secrets: []const Secret) !WebhookReceiver {
        var arena = std.heap.ArenaAllocator.init(allocator);
        errdefer arena.deinit();
        const a = arena.allocator();
        const owned = try a.alloc(Secret, secrets.len);
        for (secrets, owned) |input, *output| output.* = .{ .key = try a.dupe(u8, input.key), .secret = try a.dupe(u8, input.secret) };
        return .{ .native = native.retain(), .arena = arena, .secrets = owned };
    }
    pub fn initKeyProvider(allocator: Allocator, native: *NativeLibrary, provider: native_mod.KeyProvider) !WebhookReceiver {
        var receiver = try initSecrets(allocator, native, &.{});
        receiver.key_provider = provider;
        return receiver;
    }
    pub fn deinit(self: *WebhookReceiver) void {
        self.native.release();
        self.arena.deinit();
        self.* = undefined;
    }
    fn lock(self: *WebhookReceiver) void {
        while (!self.callback_lock.tryLock()) std.atomic.spinLoopHint();
    }
    /// The caller keeps callback contexts alive until all active dispatches finish.
    pub fn setCallback(self: *WebhookReceiver, callback: ?WebhookCallback) void {
        self.lock();
        defer self.callback_lock.unlock();
        self.callback = callback;
    }
    pub fn dispatch(self: *WebhookReceiver, snapshot: *const WebhookSnapshot) !void {
        self.lock();
        const callback = self.callback;
        self.callback_lock.unlock();
        if (callback) |cb| try cb.deliver(cb.context, snapshot);
    }
    pub fn receive(self: *const WebhookReceiver, allocator: Allocator, body: []const u8, authorization: []const u8) !types.Result(WebhookEvent) {
        var temporary = std.heap.ArenaAllocator.init(allocator);
        defer temporary.deinit();
        const a = temporary.allocator();
        var secrets: std.json.ObjectMap = .empty;
        for (self.secrets) |entry| try secrets.put(a, entry.key, .{ .string = entry.secret });
        var envelope = try self.native.invoke(allocator, .{ .operation = "webhook", .options = .{
            .body = try native_mod.base64(a, body),
            .authorization = authorization,
            .secrets = std.json.Value{ .object = secrets },
            .use_key_provider = self.key_provider != null,
        } }, null, self.key_provider);
        var transferred = false;
        defer if (!transferred) envelope.deinit();
        if (!envelope.isOk()) return .{ .failure = try types.SdkError.fromValue(allocator, envelope.parsed.value) };
        const Wire = struct {
            event: []const u8,
            id: []const u8,
            created_at: []const u8,
            raw_body: []const u8,
            room: ?WebhookRoom = null,
            participant: ?WebhookParticipant = null,
            track: ?WebhookTrack = null,
            egress: ?WebhookEgress = null,
            ingress: ?WebhookIngress = null,
        };
        const wire = try std.json.parseFromValueLeaky(Wire, envelope.parsed.arena.allocator(), envelope.value(), .{ .ignore_unknown_fields = true });
        const snapshot = WebhookSnapshot{ .event = wire.event, .id = wire.id, .created_at = try std.fmt.parseInt(i64, wire.created_at, 10), .raw_body = wire.raw_body, .room = wire.room, .participant = wire.participant, .track = wire.track, .egress = wire.egress, .ingress = wire.ingress };
        transferred = true;
        return .{ .value = .{ .owner = envelope.parsed, .snapshot = snapshot } };
    }
    pub fn receiveAndDispatch(self: *WebhookReceiver, allocator: Allocator, body: []const u8, authorization: []const u8) !types.Result(WebhookEvent) {
        var result = try self.receive(allocator, body, authorization);
        errdefer result.deinit();
        if (result == .value) try self.dispatch(&result.value.snapshot);
        return result;
    }
};
