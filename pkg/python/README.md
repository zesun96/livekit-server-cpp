# Python server binding

Python 3.10+ binding for the LiveKit server C++ SDK, loaded directly through its public C ABI.
It exposes all six server services, 52 model-based methods, 86 distinct protocol model types,
Video/SIP/Agent token grants, verified webhook snapshots and synchronous callbacks. It uses only
Python's standard library at runtime. No Python development headers, generated protobuf module,
Python-specific C++ bridge or third-party Python HTTP/crypto implementation is required.

The distribution name is `livekit-server-cpp`; import `livekit_server`. This module is separate
from the official `livekit` Python namespace. It administers the server; RTC/media clients belong
to the separate client SDK.

## Build and install

Build the existing SDK as a shared library exporting `lks_abi_version`, `lks_invoke`, `lks_free`.
Use native dependencies for the same architecture and MSVC runtime. Run from the repository root:

```powershell
cmake -S . -B out/build/python-native -G "Visual Studio 17 2022" -A x64 `
  -DBUILD_SHARED_LIBS=ON `
  -DLIVEKIT_SERVER_BUILD_TESTS=OFF -DLIVEKIT_SERVER_BUILD_INTEGRATION_TESTS=OFF `
  -DLIVEKIT_SERVER_BUILD_EXAMPLES=OFF -DLIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER=OFF `
  -DLIVEKIT_PROTOCOL_ROOT="../livekit-client-cpp/protocol" `
  -DCMAKE_PREFIX_PATH="../livekit-client-cpp/out/build/vs2022-x64-release/vcpkg_installed/x64-windows"
cmake --build out/build/python-native --config Release --parallel
$env:LIVEKIT_SERVER_PYTHON_LIBRARY = "$PWD/out/build/python-native/Release/livekitserver.dll"
$env:LIVEKIT_SERVER_PYTHON_DLL_DIRECTORIES = "$PWD/../livekit-client-cpp/out/build/vs2022-x64-release/vcpkg_installed/x64-windows/bin"
python -m pip install ./pkg/python
```

Packaging installs Python code only; it does not build, download or bundle native binaries.
Build a wheel using `python -m pip wheel --no-deps ./pkg/python --wheel-dir out/dist/python`.
With build dependencies already installed, add `--no-build-isolation` for offline packaging.
A native library matching the interpreter's architecture must be deployed separately.

Set `LIVEKIT_SERVER_PYTHON_LIBRARY` to an absolute shared-library path before the first native
operation. Alternatively pass `native=NativeLibrary(path, dll_directories=("/path/to/dependencies",))` to an API,
token builder or receiver. On Windows, dependencies can be placed beside the SDK DLL; additional
directories are accepted through `LIVEKIT_SERVER_PYTHON_DLL_DIRECTORIES` (semicolon-separated) or
`dll_directories`. They are temporarily registered using `os.add_dll_directory` while loading.
The environment library path is loaded lazily and cached after the first operation. Importing
modules, constructing models and configuring clients do not load native code. Missing native
libraries report `OSError`; missing C ABI exports or incompatible versions report `ImportError`.

Without an explicit path the loader searches for `livekitserver.dll`, `liblivekitserver.so`, or
`liblivekitserver.dylib` through standard OS library discovery. Linux/macOS require libcurl >= 7.85
and OpenSSL >= 3.0 by default. Windows defaults to WinHTTP/CNG and can opt into portable backends.
See [backend configuration](../../docs/platform-backends.md). Python validation currently covers
Windows x64 / Python 3.12 with libcurl/OpenSSL; Linux/macOS and other Python versions are unvalidated.

## Server calls and models

```python
from livekit_server import LiveKitApi, models

api = LiveKitApi()  # LIVEKIT_URL and LIVEKIT_API_KEY/LIVEKIT_API_SECRET or LIVEKIT_TOKEN
room = api.room.create_room(models.CreateRoomRequest(name="support", emptyTimeout=300))
print(room.to_dict()["name"])
rooms = api.room.list_rooms()  # equivalent to models.ListRoomsRequest()
```

Services are `api.room`, `api.egress`, `api.ingress`, `api.sip`, `api.agent_dispatch`,
`api.connector`. Methods follow snake_case C++ model overload names, such as
`api.sip.create_inbound_trunk(models.CreateSIPInboundTrunkRequest(...))`.
`api.agent_dispatch.get_dispatch(dispatch_id, room)` returns a model or `None` if the server's
filtered list response is empty. See [generated methods](src/livekit_server/services.py).

Models accept keyword fields, a mapping, or a JSON string. `Model.from_json(text)` retains the
JSON string; `.json` returns it and `.to_dict()` produces a fresh Python dictionary. Snapshots
are immutable, with equality and hashing specific to the model type. Field names follow canonical
protobuf JSON (usually camelCase), bytes use standard base64, and 64-bit integer values should
use decimal strings. Unknown fields and malformed requests are rejected by C++ when used.
These wrappers expose distinct model types without generated per-field protobuf accessors.

Use `ApiOptions(url=..., api_key=..., api_secret=..., access_token=..., timeout_ms=...)` or pass
these keywords directly to `LiveKitApi`. Options are immutable. `timeout_ms` must be positive and
fit a signed 64-bit integer. The API and token builder redact credentials in their representations.
All calls are synchronous. For asyncio, use `await asyncio.to_thread(api.room.list_rooms)`;
cancelling the await does not cancel an in-flight native request. Set its timeout accordingly.
No explicit API `close()` is required because each invoke owns its native state.

## Custom HTTP transport and errors

```python
from livekit_server import ApiOptions, HttpRequest, HttpResponse, LiveKitApi

def transport(request: HttpRequest) -> HttpResponse:
    # Honor request.timeout_ms. Send request.body as binary protobuf with request.headers.
    # This offline example returns an empty ListRoomsResponse protobuf message.
    return HttpResponse(status_code=200, body=b"")

api = LiveKitApi(ApiOptions(url="http://offline.invalid", access_token="test-token",
                           transport=transport))
assert api.room.list_rooms().to_dict() == {}
```

The immutable request contains URL, method, an ordered tuple of header pairs, bytes body and
millisecond timeout. Return `HttpResponse` with an integer status and a bytes body. Successful
HTTP responses contain binary protobuf; failures normally contain Twirp JSON. HTTP responses are
not model JSON. A shared client requires thread-safe transports and key providers.

SDK failures raise `Error` with `.code` (stable numeric value), `.error_code` (`ErrorCode` enum),
`.http_status` and `.twirp_code`. Unknown future codes map to `ErrorCode.UNKNOWN` while preserving
the numeric `.code`. Original Python transport/key-provider exceptions, including `BaseException`
subclasses, are caught inside the callback and rethrown after native result cleanup. They do not
unwind through the C stack.

`ctypes.CDLL` releases the GIL for the native call and ctypes reacquires it for Python callbacks.
Callbacks run synchronously on the invoking thread; their buffers and closures stay alive until
invoke returns. Nested calls and concurrent calls use independent callback state. Every native
result is freed exactly once with `lks_free`, including SDK failures and callback exceptions.
The binding does not retain Python objects in native code or create background callback threads.

## Access tokens

```python
from datetime import timedelta
from livekit_server import AccessToken, VideoGrant, SipGrant, AgentGrant

token = (AccessToken("test-key", "test-only-secret-at-least-32-characters")
         .set_identity("alice")
         .set_valid_for(timedelta(minutes=10))
         .set_video_grant(VideoGrant(room_join=True, room="support", can_publish=False))
         .set_sip_grant(SipGrant(call=True))
         .set_agent_grant(AgentGrant(simulation_admin=True))
         .to_jwt())
```

Setters return the builder. Identity, name, metadata and a string-to-string attribute mapping are
supported. Lifetime accepts positive integer seconds or a whole-second `timedelta`. `to_jwt(now=...)`
accepts integer Unix seconds or a timezone-aware `datetime`; the default is the current time.
The native SDK checks clock bounds and expiry. Optional grant booleans default to `None` and are
omitted, preserving server defaults; explicit `False` is retained. Grants are immutable and copy
source lists. Token signing always uses the native SDK. Mutable token builders should not be
modified concurrently.

## Webhooks

```python
from livekit_server import WebhookReceiver

receiver = WebhookReceiver(secrets={"test-key": "test-only-secret-at-least-32-characters"},
                           callback=lambda event: print(event.event, event.id))
# In a server handler, pass the original request bytes and Authorization header:
# event = receiver.receive_and_dispatch(raw_body, authorization_header)
```

Choose one credential mode: `api_key`/`api_secret`, a `secrets` mapping, or
`key_provider=lambda issuer: secret_or_none`. Secrets mappings are copied. The receiver verifies
HS256, token lifetime and exact raw-body checksum before exposing `WebhookEvent`. Body accepts
bytes or UTF-8 text; do not parse or reserialize an HTTP request body before verification.
Typed immutable room/participant/track/egress/ingress snapshots are optional. `.raw_body` preserves
the complete verified payload, including future fields; `.created_at` is an integer.

`receive()` verifies and returns without dispatching. `dispatch(event)` invokes the selected
Python callback; `receive_and_dispatch()` combines them. `set_callback(callable_or_none)` replaces
or clears the callback under a lock; invocation occurs outside the lock to permit re-entry.
Event callback exceptions propagate directly. No receiver HTTP listener or background worker is
created. A shared receiver requires a thread-safe key provider and callback.

## Offline validation and maintenance

From the repository root, with a shared SDK built as above:

```powershell
$env:PYTHONPATH = "$PWD/pkg/python/src"
$env:PYTHONDONTWRITEBYTECODE = '1'
python pkg/python/test/offline_smoke.py
python pkg/python/scripts/generate_api.py
```

The unittest suite covers all 52 service routes, 86 models, binary/Unicode/NUL round trips,
independent HMAC verification, grants, environment fallback, status/Twirp errors (including escaped
Unicode), result freeing, host exception identity, garbage collection, nested/concurrent calls,
webhook snapshots, tampering, lifetimes, dynamic keys and reentrant callback replacement.
It opens no sockets or physical devices. With no `LIVEKIT_SERVER_PYTHON_LIBRARY`, native tests
are explicitly skipped; supplying a broken library fails. Python-only tests always run.
Native C++ tests and the installed wheel should also be validated when changing the C ABI.

The generator reads only public C++ headers and updates the checked-in models, services and grants.
After changing the C ABI's shared operation mappings, run `python scripts/generate_c_api.py` too.
The [list_rooms example](examples/list_rooms.py) makes a read-only real-server request only when
explicitly run; real-server Python integration has not been validated.
