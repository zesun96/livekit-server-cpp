# livekit-server Java binding

Java 17 API backed by the C++ server SDK through JNI. The Java library has no third-party
dependencies. It includes all 52 model-based service methods (Room, Egress, Ingress, SIP,
AgentDispatch, Connector), 86 distinct JSON model types, access-token signing with Video/SIP/Agent
grants, and webhook verification with typed event snapshots, raw payloads, callbacks and key rotation.

## Build

Run from the `livekit-server-cpp` directory. Install a JDK containing JNI headers and set
`JAVA_HOME`. Use the same architecture, MSVC runtime and protobuf ABI for all native dependencies.
The bridge builds the C++ SDK in its own CMake tree; a separate SDK DLL is unnecessary when
`BUILD_SHARED_LIBS` is off (the default).

On Windows, use static protobuf and the static MSVC runtime. Some Java distributions bundle an
older `msvcp140.dll`; using `/MD` with those distributions can fail during DLL loading. The following
uses the existing workspace dependencies, without changing the installed Java runtime:

```powershell
cmake -S pkg/java -B out/build/java-binding-static -G "Visual Studio 17 2022" -A x64 `
  -DLIVEKIT_PROTOCOL_ROOT="../livekit-client-cpp/protocol" `
  -DCMAKE_PREFIX_PATH="../livekit-client-cpp/out/build/vs2022-x64-release/vcpkg_installed/x64-windows-static" `
  -DProtobuf_PROTOC_EXECUTABLE="../livekit-client-cpp/out/build/vs2022-x64-release/vcpkg_installed/x64-windows/tools/protobuf/protoc.exe" `
  '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>'
cmake --build out/build/java-binding-static --config Release --parallel
javac --release 17 -encoding UTF-8 -d out/build/java-classes (rg --files pkg/java/src/main/java pkg/java/src/test/java | Where-Object { $_ -like '*.java' })
java -Xcheck:jni "-Dlivekit.server.java.library=$PWD/out/build/java-binding-static/Release/livekitserver_java.dll" -cp out/build/java-classes io.livekit.server.OfflineSmoke
javac --release 17 -encoding UTF-8 -d out/build/java-main-classes (rg --files pkg/java/src/main/java | Where-Object { $_ -like '*.java' })
jar --create --file out/build/livekit-server-java-0.1.0.jar -C out/build/java-main-classes .
```

If the Java runtime lacks JNI headers, pass `-DJAVA_JNI_INCLUDE_DIR=...` with a directory containing
both `jni.h` and its platform's `jni_md.h` (the existing client binding's local build directory is
usable in this workspace). For a full JDK, CMake discovers both include directories automatically.
For other dependency layouts, point `CMAKE_PREFIX_PATH` at the matching protobuf package. Linux and
macOS use the corresponding `.so`/`.dylib`; an application-owned `HttpTransport` is required on those
platforms because the C++ SDK's default HTTP backend currently uses Windows WinHTTP.

Alternatively, run `gradle -p pkg/java build -PnativeLibrary=<absolute-native-library-path>` to
build the JAR and run the offline smoke test. Gradle builds Java sources only; build JNI with CMake
first. `gradle -p pkg/java jar sourcesJar` packages main classes and sources without running JNI.
Set `livekit.server.java.library` to an absolute native library path, or put the bridge on
`java.library.path`. Any shared native dependencies must also be discoverable by the OS loader.

## Usage

```java
import io.livekit.server.*;
import io.livekit.server.Models.*;

var api = new LiveKitApi(System.getenv("LIVEKIT_URL"),
    System.getenv("LIVEKIT_API_KEY"), System.getenv("LIVEKIT_API_SECRET"));
Room room = api.room().createRoom(new CreateRoomRequest("{\"name\":\"demo\"}"));
System.out.println(room.json());
System.out.println(api.room().listRooms().json());

var grant = new VideoGrant();
grant.roomJoin = true;
grant.room = "demo";
grant.canPublish = false; // null means unspecified; false is retained in the JWT
String jwt = new AccessToken(System.getenv("LIVEKIT_API_KEY"), System.getenv("LIVEKIT_API_SECRET"))
    .setIdentity("viewer").setVideoGrant(grant).toJwt();
```

`Models.*` are immutable, distinct types matching C++ `model::*`. Construct with JSON or use
`fromJson`; `json()` returns the payload. Requests use canonical protobuf JSON: camelCase fields,
base64 for bytes, and strings for 64-bit integer values. Native protobuf parsing validates requests
and rejects unknown fields. Responses use the same mapping. These wrappers expose JSON rather
than generated protobuf field getters. Use a JSON library of your choice to inspect fields.

`ApiOptions` supports URL, API key/secret, pre-issued access token, timeout and custom transport.
Empty URL/credentials follow C++ `LIVEKIT_*` environment fallbacks. The default timeout is 30 seconds.
Calls are synchronous and own native state for their duration; `LiveKitApi` requires no `close()`.
It is safe to share with concurrent callers if the supplied transport is thread-safe. For async
execution, use `CompletableFuture.supplyAsync` with an application executor. Cancellation cannot
interrupt an in-flight native call. `AccessToken` and grant builders are mutable and should not be
shared while being modified. Grant setters capture the supplied values; set them again after edits.

`LiveKitException` preserves `errorCode()`, numeric `code()`, `httpStatus()` and `twirpCode()`.
Java transport exceptions propagate unchanged. `HttpTransport.send` receives alternating header
name/value strings and **binary protobuf** body bytes; return an `HttpResponse` with status and
binary body, or the server's JSON error body on HTTP failures. The callback runs on the calling
thread, and must honor the supplied timeout itself.

## Webhooks

```java
var receiver = new WebhookReceiver(System.getenv("LIVEKIT_API_KEY"),
    System.getenv("LIVEKIT_API_SECRET"));
receiver.setCallback(event -> {
    if (event.event().equals("room_started") && event.room() != null) {
        System.out.println(event.room().name());
    }
});
// In your HTTP handler: use the unmodified request body, before decoding or reserializing it.
// receiver.receiveAndDispatch(requestBodyBytes, authorizationHeader);
```

`receive` verifies signature, JWT times and body checksum before returning a `WebhookEvent`.
`rawBody()` retains the complete verified payload, including future fields. Optional room,
participant, track, egress and ingress records are null when absent. `receiveAndDispatch` verifies
then invokes the current callback synchronously. `dispatch` invokes the callback for an already
obtained event. Callback exceptions propagate to the caller. A receiver can use a map of API
key/secret pairs, or a `Function<String, String>` that resolves an issuer's current secret and
returns null for unknown issuers. Callback replacement is thread-safe; key providers must support
concurrent access when the receiver is shared.

## Validation and maintenance

The offline smoke test opens no network connections or devices. It covers all service routes,
binary requests and responses, Unicode, independent HMAC verification, optional grants, error
details, Java exception propagation, concurrent calls, webhook checksums, key lookup and callbacks.
Real-server integration is opt-in and is not part of this test. Gradle itself is not required to
run it; the `javac`/`java` commands above use only the JDK.

After adding model-based methods to C++ headers, regenerate the checked-in Java service/model
wrappers and native dispatch table with:

```powershell
python pkg/java/scripts/generate_api.py
clang-format -i pkg/java/src/main/cpp/livekit_server_jni.cpp
```

Windows JNI build and offline tests have been exercised locally. Linux/macOS and real-server
behavior require separate validation.

See [examples](examples/README.md) for a minimal server request.
