const std = @import("std");
const native_mod = @import("native.zig");
const NativeLibrary = native_mod.NativeLibrary;
const Allocator = std.mem.Allocator;
const services = @import("services.zig");
const models = @import("models.zig");
const Result = @import("result.zig").Result;

pub const ApiOptions = struct {
    url: []const u8 = "",
    api_key: []const u8 = "",
    api_secret: []const u8 = "",
    access_token: []const u8 = "",
    timeout_ms: i64 = 30000,
    transport: ?native_mod.Transport = null,
};

/// Owns option string snapshots and one module reference. Borrow this object across calls;
/// do not bit-copy or deinit while another thread uses it. Transport contexts remain host-owned.
pub const LiveKitApi = struct {
    arena: std.heap.ArenaAllocator,
    native: *NativeLibrary,
    options: ApiOptions,

    pub fn init(allocator: Allocator, native: *NativeLibrary, options: ApiOptions) !LiveKitApi {
        if (options.timeout_ms <= 0) return error.InvalidTimeout;
        var arena = std.heap.ArenaAllocator.init(allocator);
        errdefer arena.deinit();
        const a = arena.allocator();
        var owned = options;
        owned.url = try a.dupe(u8, options.url);
        owned.api_key = try a.dupe(u8, options.api_key);
        owned.api_secret = try a.dupe(u8, options.api_secret);
        owned.access_token = try a.dupe(u8, options.access_token);
        return .{ .arena = arena, .native = native.retain(), .options = owned };
    }
    pub fn deinit(self: *LiveKitApi) void {
        self.native.release();
        self.arena.deinit();
        self.* = undefined;
    }

    pub fn call(self: *const LiveKitApi, allocator: Allocator, comptime Response: type, operation: []const u8, request: []const u8) !Result(Response) {
        var temporary = std.heap.ArenaAllocator.init(allocator);
        defer temporary.deinit();
        var envelope = try self.native.invoke(allocator, .{ .operation = operation, .request = request, .options = try self.fields(temporary.allocator()) }, self.options.transport, null);
        defer envelope.deinit();
        return envelope.into(Response, allocator);
    }
    fn fields(self: *const LiveKitApi, allocator: Allocator) !struct {
        url: []const u8,
        api_key: []const u8,
        api_secret: []const u8,
        access_token: []const u8,
        timeout_ms: []const u8,
        use_transport: bool,
    } {
        return .{ .url = self.options.url, .api_key = self.options.api_key, .api_secret = self.options.api_secret, .access_token = self.options.access_token, .timeout_ms = try std.fmt.allocPrint(allocator, "{d}", .{self.options.timeout_ms}), .use_transport = self.options.transport != null };
    }
    pub fn lookupDispatch(self: *const LiveKitApi, allocator: Allocator, dispatch_id: []const u8, room_name: []const u8) !Result(?models.AgentDispatch) {
        var temporary = std.heap.ArenaAllocator.init(allocator);
        defer temporary.deinit();
        var envelope = try self.native.invoke(allocator, .{ .operation = "get_dispatch", .dispatch_id = dispatch_id, .room = room_name, .options = try self.fields(temporary.allocator()) }, self.options.transport, null);
        defer envelope.deinit();
        if (!envelope.isOk()) return .{ .failure = try @import("result.zig").SdkError.fromValue(allocator, envelope.parsed.value) };
        const value = envelope.value();
        if (value == .null) return .{ .value = null };
        if (value != .string) return error.ExpectedNativeString;
        return .{ .value = try models.AgentDispatch.initJson(allocator, value.string) };
    }
    pub fn room(self: *const LiveKitApi) services.RoomServiceClient {
        return .{ .api = self };
    }
    pub fn egress(self: *const LiveKitApi) services.EgressClient {
        return .{ .api = self };
    }
    pub fn ingress(self: *const LiveKitApi) services.IngressClient {
        return .{ .api = self };
    }
    pub fn sip(self: *const LiveKitApi) services.SipClient {
        return .{ .api = self };
    }
    pub fn agentDispatch(self: *const LiveKitApi) services.AgentDispatchClient {
        return .{ .api = self };
    }
    pub fn connector(self: *const LiveKitApi) services.ConnectorClient {
        return .{ .api = self };
    }
};
