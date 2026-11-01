const std = @import("std");
const sdk = @import("livekit_server");
const options = @import("consumer_options");

fn send(_: ?*anyopaque, request: sdk.HttpRequest, _: std.mem.Allocator) anyerror!sdk.HttpResponse {
    try std.testing.expectEqualStrings("http://consumer.invalid/twirp/livekit.RoomService/ListRooms", request.url);
    return .{ .status_code = 200 };
}
test "separate package imports module signs token and makes an offline RPC" {
    const a = std.testing.allocator;
    const n = try sdk.NativeLibrary.open(a, options.native_library orelse return error.MissingNativeTestLibrary);
    defer n.release();
    var token = try sdk.AccessToken.init(a, n, "consumer-test-key", "consumer-test-secret-at-least-32-characters");
    defer token.deinit();
    try token.setIdentity("alice");
    try token.setVideoGrant(.{ .room_join = true, .room = "room", .can_publish = false });
    var jwt = try token.toJwtAt(a, 1700000000);
    defer jwt.deinit();
    try std.testing.expect(jwt == .value);
    try std.testing.expectEqual(@as(usize, 2), std.mem.count(u8, jwt.value.bytes, "."));
    var api = try sdk.LiveKitApi.init(a, n, .{ .url = "http://consumer.invalid", .access_token = "test-token", .transport = .{ .send = send } });
    defer api.deinit();
    var request = try sdk.models.ListRoomsRequest.empty(a);
    defer request.deinit();
    var result = try api.room().listRooms(a, request);
    defer result.deinit();
    try std.testing.expect(result == .value);
    try std.testing.expectEqualStrings("{}", result.value.json);
}
