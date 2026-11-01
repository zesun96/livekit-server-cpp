const std = @import("std");
const Allocator = std.mem.Allocator;

/// The comptime name keeps all protocol model types distinct. Owning models must not be bit-copied.
pub fn Model(comptime name: []const u8) type {
    return struct {
        allocator: Allocator,
        json: []const u8,
        pub const protocol_name = name;
        const Self = @This();

        pub fn initJson(allocator: Allocator, json: []const u8) !Self {
            return .{ .allocator = allocator, .json = try allocator.dupe(u8, json) };
        }
        pub fn empty(allocator: Allocator) !Self {
            return initJson(allocator, "{}");
        }
        pub fn fromValue(allocator: Allocator, value: anytype) !Self {
            const json = try std.json.Stringify.valueAlloc(allocator, value, .{});
            errdefer allocator.free(json);
            if (json.len == 0 or json[0] != '{') return error.ExpectedJsonObject;
            return .{ .allocator = allocator, .json = json };
        }
        pub fn toValue(self: Self, allocator: Allocator) !std.json.Parsed(std.json.Value) {
            var parsed = try std.json.parseFromSlice(std.json.Value, allocator, self.json, .{ .allocate = .alloc_always });
            errdefer parsed.deinit();
            if (parsed.value != .object) return error.ExpectedJsonObject;
            return parsed;
        }
        pub fn clone(self: Self, allocator: Allocator) !Self {
            return initJson(allocator, self.json);
        }
        pub fn deinit(self: *Self) void {
            self.allocator.free(self.json);
            self.* = undefined;
        }
    };
}

test "models detach input and inspection buffers and reject non-object values" {
    const a = std.testing.allocator;
    const M = Model("test");
    var input = [_]u8{ '{', '"', 'x', '"', ':', '"', 'a', '"', '}' };
    var model = try M.initJson(a, &input);
    defer model.deinit();
    input[6] = 'b';
    var parsed = try model.toValue(a);
    defer parsed.deinit();
    try std.testing.expectEqualStrings("a", parsed.value.object.get("x").?.string);
    var copy = try model.clone(a);
    defer copy.deinit();
    try std.testing.expectEqualStrings(model.json, copy.json);
    try std.testing.expect(model.json.ptr != copy.json.ptr);
    try std.testing.expectError(error.ExpectedJsonObject, M.fromValue(a, .{ 1, 2 }));
    var array = try M.initJson(a, "[]");
    defer array.deinit();
    try std.testing.expectError(error.ExpectedJsonObject, array.toValue(a));
    try std.testing.expect(Model("A") != Model("B"));
}
