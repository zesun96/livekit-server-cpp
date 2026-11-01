const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const sdk = b.addModule("livekit_server", .{
        .root_source_file = b.path("src/root.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    if (target.result.os.tag == .windows) sdk.linkSystemLibrary("kernel32", .{});
    const tests = b.addTest(.{ .root_module = sdk });
    b.step("test", "Run deterministic SDK/model/allocation checks without a native SDK").dependOn(&b.addRunArtifact(tests).step);

    const options = b.addOptions();
    options.addOption(?[]const u8, "native_library", b.option([]const u8, "native-library", "Trusted shared SDK path for offline native tests"));
    const native_mod = b.createModule(.{ .root_source_file = b.path("test/native.zig"), .target = target, .optimize = optimize });
    native_mod.addImport("livekit_server", sdk);
    native_mod.addOptions("test_options", options);
    const native_tests = b.addTest(.{ .root_module = native_mod });
    b.step("test-native", "Run opt-in native C ABI tests with in-memory HTTP").dependOn(&b.addRunArtifact(native_tests).step);

    const example_mod = b.createModule(.{ .root_source_file = b.path("examples/list_rooms.zig"), .target = target, .optimize = optimize });
    example_mod.addImport("livekit_server", sdk);
    const example = b.addExecutable(.{ .name = "list_rooms", .root_module = example_mod });
    b.installArtifact(example);
}
