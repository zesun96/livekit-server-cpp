# PHP server binding

PHP 8.2+ (64-bit) binding for the LiveKit C++ server SDK through public C ABI 1. Composer package:
`livekit/server-sdk-cpp`; namespace: `LiveKit\Server`. All six services, 52 model-based methods,
86 JSON models, Video/SIP/Agent token grants, verified webhook snapshots and synchronous binary
HTTP callbacks are available. This is a binding for this C++ repository, not the official PHP SDK.
RTC room connections and media belong to the separate client SDK.

The runtime requires `ext-ffi` and JSON, with no third-party PHP dependencies. The package contains
PHP source only; it does not build, download or bundle native binaries. Windows x64 / PHP 8.5.11
CLI is validated with the shared libcurl/OpenSSL SDK. PHP 8.2–8.4, other architectures, Linux/macOS,
FPM/Apache, ZTS/threaded workers and real LiveKit server integration remain unvalidated.

## Build and install

Build the shared native SDK separately, matching PHP's architecture and native dependencies:

```powershell
# From the repository root. Existing compatible shared builds can be reused.
cmake -S . -B out/build/php-native -G "Visual Studio 17 2022" -A x64 `
  -DBUILD_SHARED_LIBS=ON `
  -DLIVEKIT_SERVER_BUILD_TESTS=OFF -DLIVEKIT_SERVER_BUILD_INTEGRATION_TESTS=OFF `
  -DLIVEKIT_SERVER_BUILD_EXAMPLES=OFF -DLIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER=OFF `
  -DLIVEKIT_PROTOCOL_ROOT="../livekit-client-cpp/protocol" `
  -DCMAKE_PREFIX_PATH="../livekit-client-cpp/out/build/vs2022-x64-release/vcpkg_installed/x64-windows"
cmake --build out/build/php-native --config Release --parallel
$env:LIVEKIT_SERVER_PHP_LIBRARY = "$PWD/out/build/php-native/Release/livekitserver.dll"
$env:Path = "$PWD/../livekit-client-cpp/out/build/vs2022-x64-release/vcpkg_installed/x64-windows/bin;$env:Path"

# In pkg/php; enable ext-ffi in the PHP process running Composer.
composer install --no-dev
composer validate --strict --no-check-publish
composer archive --format=zip --dir=../../out/dist/php --file=livekit-server-sdk-cpp
```

Enable FFI in your PHP installation. For local CLI validation without changing php.ini:
`php -d extension=ffi -d ffi.enable=true test/offline_smoke.php`.
If FFI is already loaded, omit `-d extension=ffi`. Windows PHP distributions normally provide
`php_ffi.dll`; other installations may need their FFI extension installed first. Outside CLI,
this loader's `FFI::cdef` needs `ffi.enable=true` in server configuration; the default `preload`
setting restricts FFI use. See [PHP FFI configuration](https://www.php.net/manual/en/ffi.configuration.php).
This binding does not implement an FFI preloaded scope.

For application use before publication, add a Composer `path` repository pointing at the absolute
`pkg/php` directory, assign a local version and require it:

```json
{
  "repositories": [{
    "type": "path",
    "url": "/absolute/path/to/livekit-server-cpp/pkg/php",
    "options": {"symlink": false, "versions": {"livekit/server-sdk-cpp": "0.1.0"}}
  }],
  "require": {"livekit/server-sdk-cpp": "0.1.0"}
}
```

Run `composer install` in the application, then `require 'vendor/autoload.php';`. The source tree
also works without Composer through `require '/path/to/pkg/php/src/bootstrap.php';`.
Do not assume this package has been published to Packagist.

## Loading and ownership

Set `LIVEKIT_SERVER_PHP_LIBRARY` before the first SDK operation to the shared SDK's absolute path.
Alternatively pass `native: new NativeLibrary('/absolute/path/to/library')` to an API, token or
receiver. The default library is loaded lazily and cached for the PHP request/worker lifetime.
Creating models, configuring clients and loading the PHP package do not load native code.
Without a path, FFI uses OS discovery for `livekitserver.dll`, `liblivekitserver.so`, or
`liblivekitserver.dylib`. No explicit unload/close API is exposed; the FFI object owns its module.
Native load/FFI configuration failures raise runtime/FFI exceptions; an ABI version other than 1
is rejected before invocation. Never serialize native library objects.

Deploy dependency libraries on the OS loader search path **before starting PHP**. Windows can use
the process PATH shown above; portable Windows builds also need curl/OpenSSL dependency DLLs.
Linux/macOS use their loader paths/rpaths. Windows defaults to WinHTTP/CNG; Linux/macOS default to
libcurl/OpenSSL. These implementations still need validation on those platforms. See
[backend configuration](../../docs/platform-backends.md).

Every invocation retains its request, callback buffers and application exception until the native
call returns. Results are freed with that SDK's `lks_free` in `finally`, including errors. Callback
responses are PHP-owned allocations and are never freed by C++. One reusable FFI callback is
allocated per `NativeLibrary` that uses callbacks, avoiding a new trampoline on every RPC.
Reuse library instances in long-lived workers. PHP documents callback resource limitations in its
[FFI callback guide](https://www.php.net/manual/en/ffi.examples-callback.php).

## Services and models

```php
use LiveKit\Server\LiveKitApi;
use LiveKit\Server\Models;

$api = new LiveKitApi(); // LIVEKIT_URL and LIVEKIT_TOKEN or LIVEKIT_API_KEY/LIVEKIT_API_SECRET
$room = $api->room->createRoom(new Models\CreateRoomRequest(['name' => 'support']));
echo $room->toObject()->name;
$rooms = $api->room->listRooms();
```

Services are `room`, `egress`, `ingress`, `sip`, `agentDispatch`, `connector`. Method names are
lower-camel-case C++ model overload names, e.g. `sip->createInboundTrunk(...)`.
`agentDispatch->getDispatch($dispatchId, $room)` returns `Models\AgentDispatch` or null.
All models are immutable snapshots with `json`, `fromJson`, `toObject`, `JsonSerializable` and
string conversion. Models take field arrays, `stdClass`, or raw JSON strings. Object input is
serialized immediately; `toObject()` creates a detached snapshot. Raw JSON is preserved for native
schema validation on invocation. Use canonical protobuf JSON: base64 for bytes and decimal strings
for 64-bit integers. Use `(object) []` for an empty nested map and `[]` for an empty nested list.
The C++ `Empty` model is called **`EmptyMessage`** because `empty` is a reserved PHP keyword.
There are no generated per-field protobuf accessors or generated PHP protobuf dependencies.

```php
use LiveKit\Server\{ApiOptions, HttpRequest, HttpResponse, LiveKitApi};

$api = new LiveKitApi(new ApiOptions(
    url: 'https://example.livekit.cloud',
    accessToken: getenv('LIVEKIT_TOKEN') ?: '',
    timeoutMs: 5000,
    transport: function (HttpRequest $request): HttpResponse {
        // Your HTTP library must send method/url, ordered header pairs and binary body exactly,
        // honor timeoutMs, and verify TLS. Return binary protobuf on success, Twirp JSON on error.
        return applicationHttpExchange($request);
    },
));
```

Omit `transport` to use the native default HTTP backend. PHP strings carry binary bodies, including
NUL bytes. Transport callbacks must return `HttpResponse`; key providers return string or null.
`LiveKit\Server\Error` retains `errorCode`, `httpStatus`, `twirpCode` and the message (`getCode()`
also returns the numeric category). Error constants match ABI 1, and unknown future codes are
preserved. Host callback `Throwable` instances are caught at the FFI boundary and rethrown unchanged
after native result cleanup. Webhook event callbacks execute only after successful verification.

Calls block the current PHP execution thread. Synchronous nested calls, including calls on the
same library from a callback, are supported. Do not suspend a Fiber, yield to an event loop, or
share an instance across native threads while inside a native callback. There are no background
callbacks, built-in asynchronous calls or cancellation; use isolated application workers.

## Tokens and webhooks

```php
use LiveKit\Server\{AccessToken, VideoGrant, WebhookReceiver};

$jwt = (new AccessToken(getenv('LIVEKIT_API_KEY'), getenv('LIVEKIT_API_SECRET')))
    ->setIdentity('alice')->setName('Alice')->setValidFor(3600)
    ->setAttributes(['role' => 'support'])
    ->setVideoGrant(new VideoGrant(roomJoin: true, room: 'support', canPublish: false))
    ->toJwt();

$receiver = new WebhookReceiver(
    apiKey: getenv('LIVEKIT_API_KEY'), apiSecret: getenv('LIVEKIT_API_SECRET'),
    callback: function ($event): void { echo $event->event; },
);
$event = $receiver->receiveAndDispatch($rawHttpBody, $authorizationHeader);
```

`setMetadata`, `setSipGrant(new SipGrant(...))` and `setAgentGrant(new AgentGrant(...))` are supported.
Optional grant booleans default to null (omitted); explicit false is preserved. Grant fields and
the token builder use camelCase PHP names; serialized grants use ABI snake_case. Signing uses C++.
`toJwt()` accepts optional integer Unix seconds or a `DateTimeInterface` for a supplied clock.
Lifetimes are positive whole seconds; time/overflow validation also occurs in C++.

Pass exactly one of key/secret, `secrets: ['key' => 'secret', ...]`, or
`keyProvider: fn (string $key): ?string => resolveSecret($key)` to the receiver. Verification uses
the exact raw HTTP body bytes, never a reserialized JSON object. `receive()` verifies without
dispatch; `dispatch($event)` invokes the current callback; `setCallback(null)` clears it.
Callbacks can replace themselves and reenter dispatch. Events expose `event`, `id`, integer
`createdAt`, complete `rawBody`, and optional immutable `room`, `participant`, `track`, `egress`,
`ingress` records. Unknown protocol fields remain in `rawBody`.

## Offline validation

From the repository root, configure the library path/dependencies, then run:

```powershell
php -d extension=ffi -d ffi.enable=true pkg/php/test/offline_smoke.php
python pkg/php/scripts/generate_api.py
```

The offline suite covers all 52 routes and 86 models, binary/Unicode/NUL values, immutable
snapshots, independent PHP HMAC token/webhook verification, tampering/lifetime rejection,
error details, host exception identity, nested callbacks and GC. Without
`LIVEKIT_SERVER_PHP_LIBRARY`, PHP-only checks run and native checks are explicitly skipped.
Set `LIVEKIT_SERVER_PHP_AUTOLOAD` to an installed consumer's `vendor/autoload.php` to run the
same suite against the Composer-installed package. No sockets, LiveKit server or physical devices
are used. [The room-listing example](examples/list_rooms.php) contacts a real server only when run
explicitly; real-server integration has not been exercised.
