# Language binding API support

This directory contains bindings for the [LiveKit Server C++ SDK](../README.md). All five bindings
call the native SDK and cover all 52 model-based service methods and 86 protocol model types.
They expose backend administration, access-token signing, and webhook verification. RTC room
connections, media capture, playback, and client track publication belong to the separate
`livekit-client-cpp` SDK.

`Yes` means a public binding API is available; `No` means no corresponding API is provided.
API availability does not imply validation on every platform or against a real server.

| API capability | [Java 17](java/README.md) | [Ruby 3.1+](ruby/README.md) | [Python 3.10+](python/README.md) | [.NET 8+](dotnet/README.md) | [PHP 8.2+](php/README.md) |
| --- | --- | --- | --- | --- | --- |
| Room creation, listing, deletion, and metadata | Yes | Yes | Yes | Yes | Yes |
| Participant administration, subscriptions, track muting, data sending, forwarding, moving, and RPC | Yes | Yes | Yes | Yes | Yes |
| Egress start, layout/stream updates, listing, and stop | Yes | Yes | Yes | Yes | Yes |
| Ingress creation, updates, listing, and deletion | Yes | Yes | Yes | Yes | Yes |
| SIP trunks, dispatch rules, participant creation, and transfer | Yes | Yes | Yes | Yes | Yes |
| Agent dispatch creation, deletion, listing, and lookup | Yes | Yes | Yes | Yes | Yes |
| WhatsApp and Twilio connectors | Yes | Yes | Yes | Yes | Yes |
| Distinct request/response types with canonical protobuf JSON | Yes | Yes | Yes | Yes | Yes |
| Model construction and inspection with language-native mapping/Hash values | No | Yes | Yes | Object / JsonElement | Array / stdClass |
| Generated per-field protocol model getters | No | No | No | No | No |
| Synchronous server API calls | Yes | Yes | Yes | Yes | Yes |
| Built-in asynchronous server API methods | No | No | No | No | No |
| API key/secret, pre-issued access token, environment fallback, and timeout options | Yes | Yes | Yes | Yes | Yes |
| Application-owned HTTP transport with binary protobuf bodies | Yes | Yes | Yes | Yes | Yes |
| SDK errors retaining category, HTTP status, and Twirp code | Yes | Yes | Yes | Yes | Yes |
| Access-token identity, name, metadata, attributes, lifetime, and supplied clock | Yes | Yes | Yes | Yes | Yes |
| Video, SIP, and Agent grants, including optional boolean values | Yes | Yes | Yes | Yes | Yes |
| Webhook signature, lifetime, and exact-body checksum verification | Yes | Yes | Yes | Yes | Yes |
| Typed webhook snapshots and complete verified raw payload | Yes | Yes | Yes | Yes | Yes |
| Synchronous webhook callback and callback replacement | Yes | Yes | Yes | Yes | Yes |
| Multiple webhook keys and dynamic secret lookup | Yes | Yes | Yes | Yes | Yes |
| Propagation of application transport, key-provider, and webhook callback exceptions | Yes | Yes | Yes | Yes | Yes |

Java models accept JSON strings and return JSON through `json()`; use an application JSON library
to inspect fields. Ruby models expose `json` and `to_h`; Python models accept mappings, keyword
fields or JSON and expose `.json` and `.to_dict()`. .NET models accept JSON/object values and
expose `.Json` and `.ToJsonElement()`. PHP models accept arrays/objects or JSON and expose
`->json` and `->toObject()`. PHP names the reserved `Empty` type `EmptyMessage`. All five use
the [SDK-owned model boundary](../docs/public-api-boundary.md): canonical protobuf JSON field names,
base64 for bytes, and strings for 64-bit integer values. Native request parsing rejects unknown
fields. The transport callback's body is **binary protobuf**, not model JSON.

For asynchronous calls, Java applications can use `CompletableFuture.supplyAsync` with their own
executor, Ruby applications can use threads, Python applications can use `asyncio.to_thread`,
and .NET applications can use their own tasks/workers. PHP requires isolated workers and
must not suspend a Fiber or yield from an FFI callback; threaded execution is not validated.
These wrappers cannot cancel an in-flight native request. Shared APIs require thread-safe
application transports and key providers. Webhook callbacks execute synchronously on the calling thread.

## Build and packaging

| Item | [Java](java/README.md#build) | [Ruby](ruby/README.md#build-and-run) | [Python 3.10+](python/README.md) | [.NET 8+](dotnet/README.md) | [PHP](php/README.md) |
| --- | --- | --- | --- | --- | --- |
| Language entry point | `io.livekit.server.LiveKitApi` | `require 'livekit/server'`, then `LiveKit::Server::LiveKitApi` | `from livekit_server import LiveKitApi` | `LiveKit.Server.LiveKitApi` | `LiveKit\Server\LiveKitApi` |
| Native boundary | JNI adapter over the shared C ABI | Compatibility adapter over the shared C ABI | ctypes directly over the public C ABI | Managed interop delegates over public C ABI | FFI directly over public C ABI |
| Native build | `cmake -S pkg/java -B <build-dir>` | `cmake -S pkg/ruby -B <build-dir>` | Shared main SDK, `BUILD_SHARED_LIBS=ON`; no Python bridge | Shared main SDK, `BUILD_SHARED_LIBS=ON` | Shared main SDK, `BUILD_SHARED_LIBS=ON` |
| Windows bridge filename | `livekitserver_java.dll` | `livekitserver_ruby.dll` | `livekitserver.dll` | `livekitserver.dll` | `livekitserver.dll` |
| Explicit bridge path | JVM property `livekit.server.java.library` | Environment variable `LIVEKIT_SERVER_RUBY_LIBRARY` | Environment variable `LIVEKIT_SERVER_PYTHON_LIBRARY` | `LIVEKIT_SERVER_DOTNET_LIBRARY` | `LIVEKIT_SERVER_PHP_LIBRARY` |
| Language package | JAR via JDK tools or Gradle | `livekit-server-cpp` gem via `gem build` | `livekit-server-cpp` wheel via pip | `LiveKit.Server.Cpp` NuGet via `dotnet pack` | `livekit/server-sdk-cpp` via Composer |
| Runtime language dependencies | None beyond Java 17+ | Fiddle, JSON, and Base64 gems | None beyond Python 3.10+ | None beyond .NET 8+ | PHP 8.2+ (64-bit), FFI and JSON |
| Native development headers | JDK JNI headers | No Ruby development headers required | No Python development headers required | No .NET native headers required | No PHP development headers required |
| Automatic native SDK build by language packaging | No | No | No | No | No |

Run the native build separately before using the JAR, gem, Python wheel, NuGet package or Composer package. The per-language guides
provide full commands using the existing workspace protobuf and protocol checkout. The bridge architecture
must match the language process, and all linked native libraries must use compatible compiler,
runtime, and protobuf configurations. The Java/Ruby Windows guides use static protobuf and the
static MSVC runtime. This also avoids conflicts with older MSVC DLLs bundled by some Java distributions.

The Java and Ruby guides build the C++ SDK within the bridge's CMake tree. With a static SDK,
no separate
`livekitserver` DLL is required. The Ruby bridge requires `BUILD_SHARED_LIBS=OFF`. Native API state
is scoped to each call; `LiveKitApi` objects do not require `close` or disposal. Python loads the
shared SDK directly, as do .NET and PHP. Their guides document shared-library and dependency deployment.
.NET also supports explicit `NativeServer` ownership: safe module leases protect in-flight calls
through result/callback cleanup when the owner is disposed.

## Examples matrix

`Yes` means a runnable program or offline smoke scenario exists. Offline service-route checks use
fake transports; they do not establish real-server behavior.

| Example or test area | [Java](java/examples/README.md) | [Ruby](ruby/README.md#tests-and-maintenance) | [Python 3.10+](python/README.md) | [.NET 8+](dotnet/README.md) | [PHP](php/README.md) |
| --- | --- | --- | --- | --- | --- |
| Standalone room listing with environment credentials | [ListRooms.java](java/examples/java/io/livekit/server/examples/ListRooms.java) | [list_rooms.rb](ruby/examples/list_rooms.rb) | [list_rooms.py](python/examples/list_rooms.py) | [Program.cs](dotnet/examples/ListRooms/Program.cs) | [list_rooms.php](php/examples/list_rooms.php) |
| All 52 service routes with an in-memory transport | Yes, offline smoke | Yes, offline smoke | Yes, offline smoke | Yes, offline smoke | Yes |
| Binary protobuf and Unicode request/response round trips | Yes, offline smoke | Yes, offline smoke | Yes, offline smoke | Yes, offline smoke | Yes |
| Token grants and independent HMAC verification | Yes, offline smoke | Yes, offline smoke | Yes, offline smoke | Yes, offline smoke | Yes |
| Webhook verification, snapshots, key lookup, and callbacks | Yes, offline smoke | Yes, offline smoke | Yes, offline smoke | Yes, offline smoke | Yes |
| Native error details and application callback exceptions | Yes, offline smoke | Yes, offline smoke | Yes, offline smoke | Yes, offline smoke | Yes |
| Concurrent native calls | Yes, offline smoke | Yes, offline smoke | Yes, offline smoke | Yes, offline smoke | No; nested synchronous calls only |
| Host garbage collection during native callbacks | Not applicable | Yes, offline smoke | Yes, offline smoke | Yes, offline smoke | Yes |
| Automated real-server binding integration test | No | No | No | No | No |

The standalone room-listing programs require `LIVEKIT_URL` and either `LIVEKIT_TOKEN` or
`LIVEKIT_API_KEY`/`LIVEKIT_API_SECRET`. They read server state without creating rooms and contact
the server only when explicitly run. The C++ SDK's opt-in integration suite is documented in the
[root README](../README.md#tests).

## Offline validation

After building the corresponding bridge and compiling Java classes as described in its guide,
run these commands from the `livekit-server-cpp` directory:

```powershell
java -Xcheck:jni "-Dlivekit.server.java.library=$PWD/out/build/java-binding-static/Release/livekitserver_java.dll" -cp out/build/java-classes io.livekit.server.OfflineSmoke

$env:LIVEKIT_SERVER_RUBY_LIBRARY = "$PWD/out/build/ruby-binding-static/Release/livekitserver_ruby.dll"
ruby -Ipkg/ruby/lib pkg/ruby/test/offline_smoke.rb

# Build/configure the shared SDK and dependency directories as in the Python guide first.
$env:LIVEKIT_SERVER_PYTHON_LIBRARY = "$PWD/out/build/python-native/Release/livekitserver.dll"
$env:PYTHONPATH = "$PWD/pkg/python/src"
python pkg/python/test/offline_smoke.py

# Configure the shared SDK path and dependency directories as in the .NET guide first.
dotnet run --project pkg/dotnet/test/OfflineSmoke -c Release

# Configure LIVEKIT_SERVER_PHP_LIBRARY and native dependency loader paths as in the PHP guide.
php -d extension=ffi -d ffi.enable=true pkg/php/test/offline_smoke.php
```

The [Java smoke test](java/src/test/java/io/livekit/server/OfflineSmoke.java) passed 303 checks.
The [Ruby smoke test](ruby/test/offline_smoke.rb) passed 365 checks from both the source tree and
an installed gem. Local validation used Windows x64, Java classes compiled with `--release 17`,
and Ruby 4.0.7. Java JAR/example compilation, Ruby gem packaging/installation, and regeneration
checks were also exercised. Gradle execution, other Ruby versions, Linux/macOS, and real-server
binding integration have not been validated.

The [Python smoke test](python/test/offline_smoke.py) passes 17 test cases with all 52 service
routes, 86 models, native result freeing, nested/concurrent callbacks and webhook verification.
Local validation uses Python 3.12 on Windows x64 with the shared libcurl/OpenSSL SDK. Source and
installed-wheel execution are checked; other Python versions and target platforms remain unvalidated.

The [.NET smoke test](dotnet/test/OfflineSmoke/Program.cs) passes 514 checks on Windows x64 / .NET 8,
including all service methods, callbacks, GC, native module disposal, and independent webhook/token
verification. The same source also runs against the locally packed NuGet assembly through
[PackageConsumer](dotnet/test/PackageConsumer/PackageConsumer.csproj). It has no external test-package
dependency and opens no sockets/devices. Other .NET versions, platforms and real-server integration
remain unvalidated.

The [PHP smoke test](php/test/offline_smoke.php) passes 814 checks on Windows x64 / PHP 8.5.11
CLI, including every service route/model, binary/Unicode/NUL payloads, independent HMAC checks,
webhook rejection, exception identity, nested callbacks and GC. The same suite runs against a
Composer-installed copy. Package validation and archive creation are checked. Other PHP versions,
Linux/macOS, FPM/Apache, ZTS/threaded execution and real-server integration remain unvalidated.

## Platform support matrix

This table follows the [native SDK platform status](../README.md#platform-status). `Validated`
means the binding's native build and offline tests ran locally. `Limited / unvalidated` means
portable backends exist and the binding has not been exercised there. `No target` means no
supported target is declared. A loader filename or portable language code does not establish
full SDK support.

| Platform | Java | Ruby | Python | .NET | PHP |
| --- | --- | --- | --- | --- | --- |
| Windows x64 | Validated | Validated | Validated (Python 3.12) | Validated (.NET 8) | Validated (PHP 8.5 CLI) |
| GNU/Linux | Limited / unvalidated | Limited / unvalidated | Limited / unvalidated | Limited / unvalidated | Limited / unvalidated |
| macOS | Limited / unvalidated | Limited / unvalidated | Limited / unvalidated | Limited / unvalidated | Limited / unvalidated |
| iOS | No target | No target | No target | No target | No target |
| Android | No target | No target | No target | No target | No target |

Windows defaults to WinHTTP/CNG. Linux/macOS default to libcurl/OpenSSL for HTTP(S),
signing and verification. Both portable backends can be selected on Windows for local checks;
Linux/macOS builds and runtime behavior still need platform validation. See
[backend configuration](../docs/platform-backends.md) and the [common C ABI](../docs/c-abi.md).
