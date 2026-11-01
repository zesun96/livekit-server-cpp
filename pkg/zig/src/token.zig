const std = @import("std");
const native_mod = @import("native.zig");
const NativeLibrary = native_mod.NativeLibrary;
const Allocator = std.mem.Allocator;
const types = @import("result.zig");
const grants = @import("grants.zig");

/// An owning builder. Setters snapshot all input data in an arena; deinit once, do not bit-copy.
pub const AccessToken = struct {
    native: *NativeLibrary,
    arena: std.heap.ArenaAllocator,
    fields: std.json.ObjectMap,

    pub const Attribute = struct { key: []const u8, value: []const u8 };

    pub fn init(allocator: Allocator, native: *NativeLibrary, api_key: []const u8, api_secret: []const u8) !AccessToken {
        var arena = std.heap.ArenaAllocator.init(allocator);
        errdefer arena.deinit();
        const a = arena.allocator();
        var fields: std.json.ObjectMap = .empty;
        try fields.put(a, "api_key", .{ .string = try a.dupe(u8, api_key) });
        try fields.put(a, "api_secret", .{ .string = try a.dupe(u8, api_secret) });
        try fields.put(a, "valid_for", .{ .string = "21600" });
        return .{ .native = native.retain(), .arena = arena, .fields = fields };
    }
    pub fn deinit(self: *AccessToken) void {
        self.native.release();
        self.arena.deinit();
        self.* = undefined;
    }
    fn string(self: *AccessToken, name: []const u8, value: []const u8) !void {
        try self.fields.put(self.arena.allocator(), name, .{ .string = try self.arena.allocator().dupe(u8, value) });
    }
    pub fn setIdentity(self: *AccessToken, value: []const u8) !void {
        try self.string("identity", value);
    }
    pub fn setName(self: *AccessToken, value: []const u8) !void {
        try self.string("name", value);
    }
    pub fn setMetadata(self: *AccessToken, value: []const u8) !void {
        try self.string("metadata", value);
    }
    pub fn setValidFor(self: *AccessToken, seconds: i64) !void {
        if (seconds <= 0) return error.InvalidLifetime;
        try self.string("valid_for", try std.fmt.allocPrint(self.arena.allocator(), "{d}", .{seconds}));
    }
    pub fn setAttributes(self: *AccessToken, attributes: []const Attribute) !void {
        const a = self.arena.allocator();
        var fields: std.json.ObjectMap = .empty;
        for (attributes) |attribute| try fields.put(a, try a.dupe(u8, attribute.key), .{ .string = try a.dupe(u8, attribute.value) });
        try self.fields.put(a, "attributes", .{ .object = fields });
    }
    fn grant(self: *AccessToken, name: []const u8, value: anytype) !void {
        const a = self.arena.allocator();
        const json = try std.json.Stringify.valueAlloc(a, value, .{ .emit_null_optional_fields = false });
        const parsed = try std.json.parseFromSlice(std.json.Value, a, json, .{});
        try self.fields.put(a, name, parsed.value);
    }
    pub fn setVideoGrant(self: *AccessToken, value: grants.VideoGrant) !void {
        try self.grant("video", value);
    }
    pub fn setSipGrant(self: *AccessToken, value: grants.SipGrant) !void {
        try self.grant("sip", value);
    }
    pub fn setAgentGrant(self: *AccessToken, value: grants.AgentGrant) !void {
        try self.grant("agent", value);
    }
    pub fn toJwt(self: *const AccessToken, allocator: Allocator, io: std.Io) !types.Result(types.OwnedString) {
        return self.toJwtAt(allocator, std.Io.Clock.real.now(io).toSeconds());
    }
    pub fn toJwtAt(self: *const AccessToken, allocator: Allocator, now: i64) !types.Result(types.OwnedString) {
        var temporary = std.heap.ArenaAllocator.init(allocator);
        defer temporary.deinit();
        const a = temporary.allocator();
        var fields = try self.fields.clone(a);
        try fields.put(a, "now", .{ .string = try std.fmt.allocPrint(a, "{d}", .{now}) });
        var envelope = try self.native.invoke(allocator, .{ .operation = "token", .options = std.json.Value{ .object = fields } }, null, null);
        defer envelope.deinit();
        return envelope.into(types.OwnedString, allocator);
    }
};
