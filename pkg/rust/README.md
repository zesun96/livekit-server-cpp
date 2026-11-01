# Rust server binding

Rust 1.87+ crate for the LiveKit C++ server SDK through public C ABI 1. Package:
`livekit-server-cpp`; import: `livekit_server_cpp`. It exposes all six services, 52 model-based
methods, 86 JSON model types, Video/SIP/Agent grants, verified webhook snapshots and custom binary
HTTP callbacks. RTC/media connections belong to the separate client SDK. This is a binding for
this repository, separate from the official LiveKit Rust SDK.

Runtime dependencies are `libloading`, `serde`, `serde_json`, and `base64`. HMAC/SHA-256 crates are
test dependencies only; production signing, verification, protobuf and HTTP run in C++.
No Rust-specific C++ bridge, build script, protobuf code generation or HTTP runtime is required.
The crate contains Rust source only and does not build, download or bundle the native SDK.

Windows x64 / Rust 1.87.0 (MSVC) is validated with the existing shared libcurl/OpenSSL SDK.
Linux/macOS, other architectures/toolchains and real LiveKit server integration remain unvalidated.
WASM/browser and mobile targets are not declared supported. Portable loader filenames do not
establish native platform support. See [native backends](../../docs/platform-backends.md).

## Build and use

Build a compatible shared SDK separately; existing workspace shared builds can be reused:

```powershell
# From the livekit-server-cpp repository root.
cmake -S . -B out/build/rust-native -G "Visual Studio 17 2022" -A x64 `
  -DBUILD_SHARED_LIBS=ON `
  -DLIVEKIT_SERVER_BUILD_TESTS=OFF -DLIVEKIT_SERVER_BUILD_INTEGRATION_TESTS=OFF `
  -DLIVEKIT_SERVER_BUILD_EXAMPLES=OFF -DLIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER=OFF `
  -DLIVEKIT_PROTOCOL_ROOT="../livekit-client-cpp/protocol" `
  -DCMAKE_PREFIX_PATH="../livekit-client-cpp/out/build/vs2022-x64-release/vcpkg_installed/x64-windows"
cmake --build out/build/rust-native --config Release --parallel
$env:LIVEKIT_SERVER_RUST_LIBRARY = "$PWD/out/build/rust-native/Release/livekitserver.dll"
$env:Path = "$PWD/../livekit-client-cpp/out/build/vs2022-x64-release/vcpkg_installed/x64-windows/bin;$env:Path"
$env:CARGO_TARGET_DIR = "$PWD/out/build/rust-target"
cargo build --manifest-path pkg/rust/Cargo.toml --locked --examples
```

For a local consumer, add a Cargo path dependency (the crate has not been published by this task):

```toml
[dependencies]
livekit-server-cpp = { path = "/absolute/path/to/livekit-server-cpp/pkg/rust" }
```

```rust
use livekit_server_cpp::{models, serde_json::json, ApiOptions, LiveKitApi, NativeLibrary};

fn main() -> livekit_server_cpp::Result<()> {
    // SAFETY: the environment and dependency search paths select a trusted ABI 1 SDK.
    let native = unsafe { NativeLibrary::from_env()? };
    let api = LiveKitApi::new(native, ApiOptions::default())?;
    let request = models::CreateRoomRequest::from_value(json!({"name": "support"}))?;
    let room = api.room().create_room(&request)?;
    println!("{}", room.to_value()?["name"]);
    Ok(())
}
```

`ApiOptions::default()` uses native `LIVEKIT_URL` and `LIVEKIT_TOKEN` or
`LIVEKIT_API_KEY`/`LIVEKIT_API_SECRET` fallback. Alternatively set explicit option fields.
Timeouts are positive signed 64-bit milliseconds; the default is 30000. Services are `room()`,
`egress()`, `ingress()`, `sip()`, `agent_dispatch()`, `connector()`. Methods use snake_case and
borrow typed requests. `list_rooms(&models::ListRoomsRequest::default())` lists all rooms.
`agent_dispatch().get_dispatch(id, room)` returns `Result<Option<models::AgentDispatch>>`.

## Native trust, ownership and errors

`unsafe NativeLibrary::load(path)` loads an explicit module; `unsafe NativeLibrary::from_env()`
uses `LIVEKIT_SERVER_RUST_LIBRARY` or OS discovery for `livekitserver.dll`, `liblivekitserver.so`,
or `liblivekitserver.dylib`. Loading foreign code is unsafe: the caller must trust the module,
initializers, destructors, dependencies and complete ABI contract. ABI version 1 is checked before
invocation, but a version number cannot prove correct signatures, buffer lifetimes, or threading.
See the method's safety documentation and [common C ABI](../../docs/c-abi.md).

After loading, all public service/token/webhook methods are safe Rust. The module is owned by
`Arc` and retained by clients, tokens and receivers. Cloning does not reload it. Active calls
borrow their owner, so module unloading cannot precede native result cleanup. There is no global
mutable default library or explicit unload method. Loading failures are returned as `Error::Load`.
Deploy matching compiler/runtime/architecture dependencies before starting the process: Windows
can use PATH as above, Linux/macOS need appropriate loader paths/rpaths. Portable Windows builds
also require curl/OpenSSL dependency libraries.

`Error::Native { code, message, http_status, twirp_code }` preserves SDK error details and unknown
future numeric codes; constants match ABI 1. Other variants distinguish load, argument, JSON,
binding-envelope and application callback errors. `Error::Callback` retains the original boxed
error (`source()` and `downcast_ref()` work). Credentials are redacted from options/token/receiver
Debug output. Applications must still protect their own logging and callback messages.

Each invocation owns independent callback state and Rust response buffers. Native result buffers
are released exactly once with the allocating library's `lks_free` using a Drop guard, including
native errors, JSON decoding failures, host errors and panics. Null results are reported without
freeing a null pointer. Transport/key-provider panics are caught at the C boundary and resumed with
their original payload **after** native return and cleanup. This requires `panic=unwind`;
`panic=abort`, aborting panic hooks, allocation aborts and process termination cannot be recovered.
The binding does not replace the application's panic hook.

## Models and transports

Model constructors are `from_json(impl Into<String>)`, `from_value(serde_json::Value)` and
`Default` (`{}`). Raw JSON is preserved for native schema validation on invocation. `from_value`
and `to_value` require an object; `json()` returns borrowed text and `to_value()` returns an owned
snapshot. Models implement Clone, Debug, Display, Eq, Hash and serde Serialize/Deserialize.
Equality and hashing compare the stored JSON text; they do not normalize equivalent JSON.
Serde serialization emits protocol objects, rather than strings containing JSON.

Use canonical protobuf JSON field names, base64 for bytes, and strings for 64-bit integers.
There are no generated per-field protocol getters; schema validation stays in the SDK. Models
and grants own their data, so later application mutations cannot alter their internal snapshots.

```rust
use livekit_server_cpp::{ApiOptions, CallbackResult, HttpRequest, HttpResponse};
use std::sync::Arc;

let options = ApiOptions {
    transport: Some(Arc::new(|request: &HttpRequest| -> CallbackResult<HttpResponse> {
        // Your HTTP library sends URL/method, ordered headers and raw binary protobuf bytes,
        // honors timeout_ms and verifies TLS. Successful response bodies are binary protobuf;
        // failed responses normally contain Twirp JSON. This stub is useful for offline checks.
        Ok(HttpResponse { status_code: 200, body: Vec::new() })
    })),
    ..Default::default()
};
```

Omit `transport` for the native HTTP backend. Callback types are `Fn + Send + Sync` and return
`CallbackResult<T>` with a boxed `std::error::Error + Send + Sync`. Request bodies are `Vec<u8>`;
NUL bytes and non-UTF-8 binary data are preserved. Callback input is borrowed for the call;
clone it if retaining it. Transports receive a shared request reference, preventing direct mutation.

Clients, modules, tokens and receivers support sharing across Rust threads. Calls are synchronous;
each callback runs on the invoking thread, including nested calls on the same native module.
No application code executes under an SDK binding callback lock. There is no built-in async API,
runtime or cancellation; use an application's blocking worker facilities for asynchronous use.

## Tokens and webhooks

```rust
use livekit_server_cpp::{AccessToken, NativeLibrary, VideoGrant, WebhookReceiver};

fn use_sdk(native: NativeLibrary, key: String, secret: String,
           raw_body: &[u8], authorization: &str) -> livekit_server_cpp::Result<()> {
    let jwt = AccessToken::new(native.clone(), key.clone(), secret.clone())
        .with_identity("alice")
        .with_valid_for(3600)?
        .with_video_grant(VideoGrant {
            room_join: true, room: "support".into(), can_publish: Some(false),
            ..Default::default()
        }).to_jwt()?;
    let receiver = WebhookReceiver::new(native, key, secret);
    let event = receiver.receive(raw_body, authorization)?;
    println!("{} {}", jwt.len(), event.event);
    Ok(())
}
```

Builders also support `with_name`, `with_metadata`, `with_attributes(BTreeMap<String, String>)`,
`with_sip_grant` and `with_agent_grant`. Optional grant booleans use `Option<bool>`: None omits
the claim, while Some(false) preserves an explicit denial. `to_jwt_at(i64)` supplies Unix seconds;
`to_jwt_at_time(SystemTime)` accepts a supplied clock and rejects pre-epoch or overflowing times.
Native code validates time, positive lifetime and expiry overflow too.

Receivers accept a key/secret pair, `with_secrets(native, BTreeMap<...>)`, or
`with_key_provider(native, Arc<KeyProvider>)`. Providers return `CallbackResult<Option<String>>`;
None means unknown issuer. Pass the exact raw HTTP body bytes to `receive`, never reserialized JSON.
Events own `event`, `id`, integer `created_at`, full verified `raw_body`, and optional typed `room`,
`participant`, `track`, `egress` and `ingress` snapshots. Unknown fields remain in `raw_body`.

`set_callback(Some(Arc<WebhookCallback>))` installs an event handler; None clears it. Cloned receivers
share this callback slot. `dispatch(&event)` invokes the current handler; `receive_and_dispatch`
verifies before delivery. Callback replacement, reentrant dispatch and application destructors
run outside the lock. Event callback errors return `Error::Callback`; event panics propagate in
Rust after native verification has already returned. Stored event callbacks must not capture their
receiver through a strong Arc cycle; use Weak when they need to reenter the same receiver.

## Offline tests and packaging

From the repository root with a Rust toolchain and cached/downloaded crates:

```powershell
$env:CARGO_TARGET_DIR = "$PWD/out/build/rust-target"
cargo test --offline --locked --manifest-path pkg/rust/Cargo.toml
# With LIVEKIT_SERVER_RUST_LIBRARY and native dependency paths configured:
cargo test --offline --locked --manifest-path pkg/rust/Cargo.toml -- --include-ignored
cargo clippy --offline --locked --manifest-path pkg/rust/Cargo.toml --all-targets -- -D warnings
cargo fmt --manifest-path pkg/rust/Cargo.toml -- --check
cargo package --offline --locked --manifest-path pkg/rust/Cargo.toml
python pkg/rust/scripts/generate_api.py
```

The default suite runs model/grant/envelope checks and an instrumented ABI fixture verifying
allocation/free counts on success, errors, null results and resumed panics. Nine native test cases
are explicitly ignored by default and require the environment path. They cover all 52 routes,
binary/Unicode/NUL data, independent HMAC/SHA-256 signing, webhook tampering/lifetimes, host errors
and panic payloads, callback replacement/destructors, and nested calls from eight concurrent threads.
Tests use in-memory transports, not sockets or a LiveKit server. HMAC/SHA-256 crates are used only
to independently check native crypto. The generated model suite covers all 86 types.

`cargo package` verifies the extracted source crate without a native SDK at build time. During
development add `--allow-dirty` to package uncommitted files. The package includes its tests and
Cargo.lock, but not C++ binaries, build trees or the generator. The standalone
[room-listing example](examples/list_rooms.rs) contacts the server only when explicitly run with
`cargo run --manifest-path pkg/rust/Cargo.toml --example list_rooms`. It has only been compiled;
real-server integration has not been exercised.
