# LiveKit server Ruby binding

Ruby API backed by the C++ server SDK through a small C ABI bridge and Fiddle. It covers all
52 model-based service methods, 86 distinct JSON model classes, Video/SIP/Agent token grants,
and webhook verification with typed snapshots, raw payloads, callbacks and rotating keys.
Ruby 3.1+ is required. The runtime dependencies are Fiddle, JSON and Base64; no Ruby development
headers or Ruby ABI-specific extension is needed. On this workspace's Windows setup, MinGW Ruby
can call the MSVC-built bridge without exchanging C++ objects or allocator ownership.

## Build and run

Run from the `livekit-server-cpp` repository directory. Build a bridge matching the architecture
of Ruby, with consistent protobuf and MSVC runtimes. The bridge embeds a static SDK; it does not
require a separate `livekitserver` shared library. Reuse the existing workspace dependencies:

```powershell
cmake -S pkg/ruby -B out/build/ruby-binding-static -G "Visual Studio 17 2022" -A x64 `
  -DLIVEKIT_PROTOCOL_ROOT="../livekit-client-cpp/protocol" `
  -DCMAKE_PREFIX_PATH="../livekit-client-cpp/out/build/vs2022-x64-release/vcpkg_installed/x64-windows-static" `
  -DProtobuf_PROTOC_EXECUTABLE="../livekit-client-cpp/out/build/vs2022-x64-release/vcpkg_installed/x64-windows/tools/protobuf/protoc.exe" `
  '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>'
cmake --build out/build/ruby-binding-static --config Release --parallel
$env:LIVEKIT_SERVER_RUBY_LIBRARY = "$PWD/out/build/ruby-binding-static/Release/livekitserver_ruby.dll"
ruby -Ipkg/ruby/lib pkg/ruby/test/offline_smoke.rb
```

For another dependency layout, point `CMAKE_PREFIX_PATH` at a protobuf installation with a matching
runtime, and set `LIVEKIT_PROTOCOL_ROOT` to a local protocol checkout. `BUILD_SHARED_LIBS` must be
off. Linux/macOS use `liblivekitserver_ruby.so`/`.dylib`; pass the built library path through the
same environment variable. The C++ SDK's default HTTP backend is Windows WinHTTP; supply a Ruby
transport on other platforms. The native bridge and Ruby code must use the same bridge ABI
(currently version 1). Library loading is lazy, and reports an incompatible ABI on first use.
The C++ SHA-256/HS256 backend currently also requires Windows: token signing and webhook
verification on other platforms report `unsupported`. Requests with a pre-issued access token
and a custom transport can avoid those backend requirements.

The gem packages the Ruby API and bridge sources, and loads an independently built native library.
It does not build or bundle the C++ SDK automatically. Build the bridge from this full repository
before installing or deploying the gem. Build the gem without downloading dependencies:

```powershell
Push-Location pkg/ruby
gem build livekit-server-cpp.gemspec --output ../../out/build/livekit-server-cpp-0.1.0.gem
Pop-Location
```

Install with `gem install --local <gem-path>` when deploying. Set `LIVEKIT_SERVER_RUBY_LIBRARY`
to the bridge path, or place its platform-specific filename on the OS loader path. Keep any
shared native dependencies discoverable by the OS loader as well. `require 'livekit/server'`
only loads Ruby definitions; the first native operation loads the bridge.

## Server requests

```ruby
require 'livekit/server'
sdk = LiveKit::Server
models = sdk::Models

# Empty URL/credentials use LIVEKIT_URL and LIVEKIT_TOKEN or LIVEKIT_API_KEY/LIVEKIT_API_SECRET.
api = sdk::LiveKitApi.new
room = api.room.create_room(models::CreateRoomRequest.new(name: 'demo'))
puts room.to_h.fetch('name')
puts api.room.list_rooms.json
```

Services are `room`, `egress`, `ingress`, `sip`, `agent_dispatch` and `connector`. Methods use
snake_case: for example `start_room_composite_egress`, `create_inbound_trunk`, `create_dispatch`,
and `dial_whats_app_call`. `agent_dispatch.get_dispatch(id, room)` returns nil when absent.
The [generated API](lib/livekit/server/generated.rb) lists the full method and request types.

Models accept a Hash or a raw JSON String. Use `json`, `to_h`, `empty?`, or `.from_json`.
Their JSON representation follows the C++ SDK's canonical protobuf mapping: camelCase field
names, base64-encoded bytes, strings for 64-bit integers, and protobuf enum names. There are no
generated per-field getters. Native protobuf parsing validates requests and rejects unknown
fields. Malformed model JSON is reported as an SDK protocol error. Model classes are distinct;
passing a request of the wrong type raises `ArgumentError` before calling native code.

Calls are synchronous. Each call owns and releases its native state. No `close` is needed.
The Fiddle call releases the GVL; transport/key callbacks execute with Ruby's GVL on the calling
thread. A shared API can be used by multiple threads if its transport is thread-safe. A Ruby
thread interruption cannot cancel an in-flight native HTTP operation; the configured timeout
still applies. `ApiOptions` is immutable and accepts `url`, `api_key`, `api_secret`, `access_token`,
`timeout_ms` (default 30,000), and `transport`. Token builders are mutable and must not be modified
concurrently. Use `inspect` for options/builders to avoid printing credentials.

## Tokens

```ruby
grant = sdk::VideoGrant.new(room_join: true, room: 'demo', can_publish: false)
token = sdk::AccessToken.new(api_key: ENV.fetch('LIVEKIT_API_KEY'),
  api_secret: ENV.fetch('LIVEKIT_API_SECRET'))
  .set_identity('viewer').set_video_grant(grant).set_valid_for(3600).to_jwt
```

Grant keyword names mirror the C++ fields in snake_case. Optional boolean values of nil are
omitted, and explicit false values are preserved. Grants are copied at construction. Token
setters also support name, metadata, attributes, SIP and Agent grants. `to_jwt(now: Time_or_seconds)`
can use a supplied clock. Lifetimes and timestamps are checked against the native clock range
before signing; timestamps before the Unix epoch are rejected.

## Webhooks

```ruby
receiver = sdk::WebhookReceiver.new(api_key: ENV.fetch('LIVEKIT_API_KEY'),
  api_secret: ENV.fetch('LIVEKIT_API_SECRET')) do |event|
  puts event.room.name if event.event == 'room_started' && event.room
end
# Inside your HTTP handler, preserve the original body bytes:
# receiver.receive_and_dispatch(raw_body, authorization_header)
```

`receive` verifies JWT signature/times and body checksum before returning a frozen `WebhookEvent`.
It exposes `event`, `id`, `created_at`, optional `room`/`participant`/`track`/`egress`/`ingress`
records, and `raw_body`, which preserves the verified payload including future fields. `dispatch`
delivers an existing event, and `receive_and_dispatch` verifies then delivers it. Set or replace
the synchronous callback with `set_callback { |event| ... }`. Callbacks execute outside the
callback mutex; they may replace themselves. Exceptions propagate unchanged.

For rotation, use `secrets: { 'key-a' => 'secret-a', ... }` or
`key_provider: ->(issuer) { lookup_secret(issuer) }`. Return nil for unknown issuers. Providers
must support concurrent access when a receiver is shared.

## Custom transport and errors

```ruby
transport = lambda do |request|
  # Send request.url/method/headers/body with request.timeout_ms.
  # headers is an array of [name, value] pairs; body contains binary protobuf, not JSON.
  # Return a real HTTP result from your application's HTTP client:
  sdk::HttpResponse.new(status_code: http_status, body: response_bytes)
end
api = sdk::LiveKitApi.new(transport: transport)
```

Transport callbacks receive frozen `HttpRequest` snapshots. Successful response bodies contain
binary protobuf; failed HTTP responses normally contain Twirp JSON. Honor the supplied timeout
in your transport. Ruby callback exceptions are caught before crossing the native stack and
rethrown unchanged when the call returns. `LiveKit::Server::Error` preserves `code`, symbolic
`error_code`, `http_status` and `twirp_code` from C++.

## Tests and maintenance

The offline smoke test exercises every service route, binary/Unicode round trips, independent
JWT verification, grants, webhook verification and typed fields, error details, callback
exceptions, concurrent calls, GC and compaction. It needs no network, server or hardware.
The read-only `examples/list_rooms.rb` uses a real server only when explicitly run:

```powershell
ruby -Ipkg/ruby/lib pkg/ruby/examples/list_rooms.rb
```

Regenerate models, services, grants and native dispatch after changing C++ headers:

```powershell
python pkg/ruby/scripts/generate_api.py
clang-format -i pkg/ruby/src/livekit_server_ruby.cpp
```

Windows x64 builds and offline tests are validated with the local Ruby installation. Other Ruby
versions, Linux/macOS, and real-server integration require separate validation.
