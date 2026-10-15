# Language binding API support

This directory contains bindings for the [LiveKit Server C++ SDK](../README.md). Both bindings
call the native SDK and cover all 52 model-based service methods and 86 protocol model types.
They expose backend administration, access-token signing, and webhook verification. RTC room
connections, media capture, playback, and client track publication belong to the separate
`livekit-client-cpp` SDK.

`Yes` means a public binding API is available; `No` means no corresponding API is provided.
API availability does not imply validation on every platform or against a real server.

| API capability | [Java 17](java/README.md) | [Ruby 3.1+](ruby/README.md) |
| --- | --- | --- |
| Room creation, listing, deletion, and metadata | Yes | Yes |
| Participant administration, subscriptions, track muting, data sending, forwarding, moving, and RPC | Yes | Yes |
| Egress start, layout/stream updates, listing, and stop | Yes | Yes |
| Ingress creation, updates, listing, and deletion | Yes | Yes |
| SIP trunks, dispatch rules, participant creation, and transfer | Yes | Yes |
| Agent dispatch creation, deletion, listing, and lookup | Yes | Yes |
| WhatsApp and Twilio connectors | Yes | Yes |
| Distinct request/response types with canonical protobuf JSON | Yes | Yes |
| Model construction and inspection with language-native Hash values | No | Yes |
| Generated per-field protocol model getters | No | No |
| Synchronous server API calls | Yes | Yes |
| Built-in asynchronous server API methods | No | No |
| API key/secret, pre-issued access token, environment fallback, and timeout options | Yes | Yes |
| Application-owned HTTP transport with binary protobuf bodies | Yes | Yes |
| SDK errors retaining category, HTTP status, and Twirp code | Yes | Yes |
| Access-token identity, name, metadata, attributes, lifetime, and supplied clock | Yes | Yes |
| Video, SIP, and Agent grants, including optional boolean values | Yes | Yes |
| Webhook signature, lifetime, and exact-body checksum verification | Yes | Yes |
| Typed webhook snapshots and complete verified raw payload | Yes | Yes |
| Synchronous webhook callback and callback replacement | Yes | Yes |
| Multiple webhook keys and dynamic secret lookup | Yes | Yes |
| Propagation of application transport, key-provider, and webhook callback exceptions | Yes | Yes |

Java models accept JSON strings and return JSON through `json()`; use an application JSON library
to inspect fields. Ruby models accept a Hash or JSON string and expose `json` and `to_h`. Both use
the [SDK-owned model boundary](../docs/public-api-boundary.md): canonical protobuf JSON field names,
base64 for bytes, and strings for 64-bit integer values. Native request parsing rejects unknown
fields. The transport callback's body is **binary protobuf**, not model JSON.

For asynchronous calls, Java applications can use `CompletableFuture.supplyAsync` with their own
executor, and Ruby applications can use threads. Neither binding can cancel an in-flight native
request through an asynchronous wrapper. Shared APIs require thread-safe application transports
and key providers. Webhook callbacks execute synchronously on the calling thread.

## Build and packaging

| Item | [Java](java/README.md#build) | [Ruby](ruby/README.md#build-and-run) |
| --- | --- | --- |
| Language entry point | `io.livekit.server.LiveKitApi` | `require 'livekit/server'`, then `LiveKit::Server::LiveKitApi` |
| Native boundary | JNI bridge linked with the C++ SDK | C ABI bridge loaded through Fiddle |
| Native build | `cmake -S pkg/java -B <build-dir>` | `cmake -S pkg/ruby -B <build-dir>` |
| Windows bridge filename | `livekitserver_java.dll` | `livekitserver_ruby.dll` |
| Explicit bridge path | JVM property `livekit.server.java.library` | Environment variable `LIVEKIT_SERVER_RUBY_LIBRARY` |
| Language package | JAR via JDK tools or Gradle | `livekit-server-cpp` gem via `gem build` |
| Runtime language dependencies | None beyond Java 17+ | Fiddle, JSON, and Base64 gems |
| Native development headers | JDK JNI headers | No Ruby development headers required |
| Automatic native SDK build by language packaging | No | No |

Run the native build separately before using the JAR or gem. The per-language guides provide full
commands using the existing workspace protobuf and protocol checkout. The bridge architecture
must match the language process, and all linked native libraries must use compatible compiler,
runtime, and protobuf configurations. The Windows guides use static protobuf and the static MSVC
runtime. This also avoids conflicts with older MSVC DLLs bundled by some Java distributions.

Both guides build the C++ SDK within the bridge's CMake tree. With a static SDK, no separate
`livekitserver` DLL is required. The Ruby bridge requires `BUILD_SHARED_LIBS=OFF`. Native API state
is scoped to each call; `LiveKitApi` objects do not require `close` or disposal.

## Examples matrix

`Yes` means a runnable program or offline smoke scenario exists. Offline service-route checks use
fake transports; they do not establish real-server behavior.

| Example or test area | [Java](java/examples/README.md) | [Ruby](ruby/README.md#tests-and-maintenance) |
| --- | --- | --- |
| Standalone room listing with environment credentials | [ListRooms.java](java/examples/java/io/livekit/server/examples/ListRooms.java) | [list_rooms.rb](ruby/examples/list_rooms.rb) |
| All 52 service routes with an in-memory transport | Yes, offline smoke | Yes, offline smoke |
| Binary protobuf and Unicode request/response round trips | Yes, offline smoke | Yes, offline smoke |
| Token grants and independent HMAC verification | Yes, offline smoke | Yes, offline smoke |
| Webhook verification, snapshots, key lookup, and callbacks | Yes, offline smoke | Yes, offline smoke |
| Native error details and application callback exceptions | Yes, offline smoke | Yes, offline smoke |
| Concurrent native calls | Yes, offline smoke | Yes, offline smoke |
| Ruby GC and compaction during native callbacks | Not applicable | Yes, offline smoke |
| Automated real-server binding integration test | No | No |

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
```

The [Java smoke test](java/src/test/java/io/livekit/server/OfflineSmoke.java) passed 303 checks.
The [Ruby smoke test](ruby/test/offline_smoke.rb) passed 365 checks from both the source tree and
an installed gem. Local validation used Windows x64, Java classes compiled with `--release 17`,
and Ruby 4.0.7. Java JAR/example compilation, Ruby gem packaging/installation, and regeneration
checks were also exercised. Gradle execution, other Ruby versions, Linux/macOS, and real-server
binding integration have not been validated.

## Platform support matrix

This table follows the [native SDK platform status](../README.md#platform-status). `Validated`
means the binding's native build and offline tests ran locally. `Limited / unvalidated` means
native backends are missing and the binding has not been exercised there. `No target` means no
supported target is declared. A loader filename or portable language code does not establish
full SDK support.

| Platform | Java | Ruby |
| --- | --- | --- |
| Windows x64 | Validated | Validated |
| GNU/Linux | Limited / unvalidated | Limited / unvalidated |
| macOS | Limited / unvalidated | Limited / unvalidated |
| iOS | No target | No target |
| Android | No target | No target |

The default HTTP transport uses WinHTTP, and SHA-256/HS256 uses Windows CNG. Non-Windows requests
need a custom transport and a pre-issued access token. Token signing and webhook verification
currently report `unsupported` there, even when a custom HTTP transport is supplied.
