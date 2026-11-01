const std = @import("std");
const Allocator = std.mem.Allocator;

pub const OwnedString = struct {
    allocator: Allocator,
    bytes: []const u8,

    pub fn initJson(allocator: Allocator, bytes: []const u8) !OwnedString {
        return .{ .allocator = allocator, .bytes = try allocator.dupe(u8, bytes) };
    }
    pub fn deinit(self: *OwnedString) void {
        self.allocator.free(self.bytes);
        self.* = undefined;
    }
};

/// Owned failure details. Numeric categories match ABI 1, including unknown future values.
pub const SdkError = struct {
    allocator: Allocator,
    code: i32,
    message: []const u8,
    http_status: i32,
    twirp_code: []const u8,

    pub const invalid_argument = 0;
    pub const authentication = 1;
    pub const transport = 2;
    pub const http = 3;
    pub const protocol = 4;
    pub const unsupported = 5;
    pub const unknown = 255;

    pub fn fromValue(allocator: Allocator, value: std.json.Value) !SdkError {
        const Wire = struct { ok: bool, code: i32, message: []const u8, http_status: i32, twirp_code: []const u8 };
        var parsed = try std.json.parseFromValue(Wire, allocator, value, .{ .ignore_unknown_fields = true });
        defer parsed.deinit();
        const message = try allocator.dupe(u8, parsed.value.message);
        errdefer allocator.free(message);
        return .{ .allocator = allocator, .code = parsed.value.code, .message = message, .http_status = parsed.value.http_status, .twirp_code = try allocator.dupe(u8, parsed.value.twirp_code) };
    }
    pub fn deinit(self: *SdkError) void {
        self.allocator.free(self.message);
        self.allocator.free(self.twirp_code);
        self.* = undefined;
    }
};

/// The outer Zig error union represents host/allocation/loader errors. This union owns either
/// a successful SDK value or complete native failure details; call deinit exactly once.
pub fn Result(comptime T: type) type {
    return union(enum) {
        value: T,
        failure: SdkError,

        pub fn deinit(self: *@This()) void {
            switch (self.*) {
                .value => |*value| if (@typeInfo(T) == .optional) {
                    if (value.*) |*item| item.deinit();
                } else value.deinit(),
                .failure => |*failure| failure.deinit(),
            }
            self.* = undefined;
        }
    };
}
