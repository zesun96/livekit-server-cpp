const std = @import("std");
const sdk = @import("livekit_server");

pub fn main(init: std.process.Init) !void {
    const a = init.gpa;
    // The application trusts the selected native SDK and its dependency loader paths.
    const native = try sdk.NativeLibrary.open(a, init.environ_map.get("LIVEKIT_SERVER_ZIG_LIBRARY") orelse sdk.NativeLibrary.default_filename);
    defer native.release();
    var api = try sdk.LiveKitApi.init(a, native, .{});
    defer api.deinit();
    var request = try sdk.models.ListRoomsRequest.empty(a);
    defer request.deinit();
    var result = try api.room().listRooms(a, request);
    defer result.deinit();
    switch (result) {
        .value => |rooms| std.debug.print("{s}\n", .{rooms.json}),
        .failure => |failure| {
            std.debug.print("SDK failure {d} (HTTP {d}): {s}\n", .{ failure.code, failure.http_status, failure.message });
            return error.NativeSdkFailure;
        },
    }
}
