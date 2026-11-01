const std = @import("std");
pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const dep = b.dependency("livekit_server_cpp", .{ .target = target, .optimize = optimize });
    const mod = b.createModule(.{ .root_source_file = b.path("consumer.zig"), .target = target, .optimize = optimize });
    mod.addImport("livekit_server", dep.module("livekit_server"));
    const options = b.addOptions();
    options.addOption(?[]const u8, "native_library", b.option([]const u8, "native-library", "Trusted SDK path"));
    mod.addOptions("consumer_options", options);
    const tests = b.addTest(.{ .root_module = mod });
    b.default_step.dependOn(&tests.step);
    b.step("test", "Run the separate source-package consumer with offline HTTP").dependOn(&b.addRunArtifact(tests).step);
}
