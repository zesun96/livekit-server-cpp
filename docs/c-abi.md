# Common C ABI, version 1

The public header `livekit/server/c_api.h` compiles as C or C++. Link
`LiveKitServer::livekitserver`; a shared build (`BUILD_SHARED_LIBS=ON`) exports
`lks_abi_version`, `lks_invoke` and `lks_free`. Windows static consumers must not define
`LIVEKIT_SERVER_C_SHARED`; the CMake target propagates the correct definition.

## Ownership, errors and callbacks

- Check `lks_abi_version() == 1` before use. Incompatible signatures or JSON contracts require
  a new ABI version; new operations and error values may be added. Handle unknown error codes.
- Pass UTF-8 JSON with an explicit byte length. Inputs remain caller-owned and are borrowed
  only for the call. Each invocation owns independent native state; there is no persistent handle.
- Every non-NULL result is a library allocation, NUL-terminated, with optional byte length
  excluding NUL. Call `lks_free` exactly once in the allocating library, including for failures.
  Never use the host runtime's free. `lks_free(NULL)` is safe.
- Success is `{"ok":true,"value":...}`. Errors are
  `{"ok":false,"code":0,"message":"...","http_status":0,"twirp_code":""}`.
  Error codes match `lks_error_code`: invalid argument 0, authentication 1, transport 2, HTTP 3,
  protocol 4, unsupported 5, unknown 255. Non-HTTP errors normally use status 0 and an empty
  Twirp code. NULL signals native allocation/encoding failure; output length is then zero.
  No exception crosses the C boundary. There is no thread-local last-error state.
- A callback runs synchronously on the invoking thread. Request buffers are borrowed only
  during that callback. Return 0 and fill `response`/`response_length`, or nonzero on failure.
  The response must stay alive until the enclosing invoke returns; the SDK copies it immediately
  and never frees it. Buffers need not be NUL-terminated. `user_data` is never retained.
- Do not unwind host exceptions or longjmp through the callback. Catch and save the exception,
  return nonzero, then rethrow in the host after invoke returns. JNI and Ruby do this.
  Nested invokes are allowed. Concurrent calls require separate or synchronized callback state.
  There are no background callbacks and none run after the call returns.

## Requests

All 52 model-based methods use `operation: "<service>.<method>"`, e.g. `Room.ListRooms`,
`Room.CreateRoom`, `Sip.CreateSIPDispatchRule`; service names are `Room`, `Egress`, `Ingress`,
`Sip`, `AgentDispatch`, `Connector`. Names match C++ client accessors and model overloads in
`include/livekit/server/`. The single shared table is `src/detail/dispatch.inc`.

```json
{"operation":"Room.CreateRoom","request":"{\"name\":\"support\"}","options":{"url":"https://example.livekit.cloud","api_key":"test-key","api_secret":"test-only-secret-at-least-32-characters","timeout_ms":"30000","use_transport":false}}
```

`request` is a string containing canonical protobuf JSON, not a nested object. The successful
`value` is also a JSON string. Options additionally accept `access_token` to use a pre-issued
JWT. `timeout_ms` is required, positive, and encoded as a decimal string. Empty URL/credentials
use the C++ SDK's `LIVEKIT_*` environment fallback. `use_transport:true` selects the callback;
otherwise the configured default HTTP backend is used.

`get_dispatch` takes `dispatch_id`, `room`, and the same API options. Its value is a JSON string
or null if no dispatch matches.

`token` accepts options `api_key`, `api_secret`, `identity`, `name`, `metadata`, `attributes`
(string map), `valid_for` (positive seconds as a string), and `now` (Unix seconds as a string).
It returns a JWT string. Optional `video`, `sip`, `agent` objects use the snake_case grant fields
from `access_token.h`. Omitted optional booleans preserve server defaults.

`webhook` accepts options `body` (unpadded base64url raw bytes), `authorization`, `secrets`
(API key -> secret map), or `use_key_provider:true`. It returns an object with `event`, `id`,
`created_at` (decimal string), `raw_body` (complete verified UTF-8 JSON) and optional `room`,
`participant`, `track`, `egress`, `ingress` convenience objects with snake_case field names.
Verification precedes delivery; the host can dispatch application events after invoke returns.

## Callback payloads

Transport request:

```json
{"kind":"transport","url":"https://example/twirp/livekit.RoomService/ListRooms","method":"POST","headers":[["Content-Type","application/protobuf"]],"body":"","timeout_ms":"30000"}
```

Transport response: `{"status_code":"200","body":""}`. Body fields are unpadded base64url
binary bytes. Successful Twirp responses are protobuf binary, failed HTTP responses are normally
Twirp JSON. The host must honor timeouts. Headers are ordered name/value pairs; response headers
are currently unused by the RPC layer. Invalid status codes or callback buffers fail the call.
`{"error":true}` also indicates a transport callback failure.

Key lookup request: `{"kind":"key","key":"issuer-api-key"}`.
Response: `{"secret":"resolved-secret"}`; an empty secret means unknown issuer.

Regenerate shared native mappings with `python scripts/generate_c_api.py` from the
repository root. Ruby compatibility exports, JNI and the [Python ctypes binding](../pkg/python/README.md) call
this same implementation. Host callback exceptions are retained and rethrown in their language
after the native invoke returns and its result has been freed.
