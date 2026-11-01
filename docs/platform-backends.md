# Portable server backends

HTTP portability and cryptography are separate requirements. libcurl handles HTTP(S), proxies,
timeouts and TLS policy; OpenSSL Crypto handles SHA-256 and HS256 used by tokens and webhook
verification. Neither dependency establishes Linux/macOS runtime support without platform tests.
This change applies to the server SDK; client WebRTC/media portability is a separate task.

| Platform | Default HTTP | Default cryptography | Validation |
| --- | --- | --- | --- |
| Windows x64 | WinHTTP | Windows CNG | Native unit tests and Ruby/JNI offline tests |
| Linux | libcurl >= 7.85 | OpenSSL >= 3.0 | Implementation provided; platform execution pending |
| macOS | libcurl >= 7.85 | OpenSSL >= 3.0 | Implementation provided; platform execution pending |

Select portable implementations on Windows using `LIVEKIT_SERVER_USE_CURL=ON` and
`LIVEKIT_SERVER_USE_OPENSSL=ON`; they are independent switches and default to OFF there.
Both default to ON and are required on non-Windows platforms. Missing dependencies fail at
configure time, rather than leaving runtime `unsupported` placeholders. vcpkg declares curl,
OpenSSL and protobuf. Windows native builds with externally supplied protobuf still need no
HTTP/crypto packages unless a portable backend is selected.

The CMake package propagates required dependencies to installed static consumers. The C ABI,
Ruby, Java, Python and .NET use the same selected backends. A custom transport remains available through
`ApiOptions::transport` or the synchronous C ABI callback.

Example configuration (use dependencies built for the same architecture and C++ runtime):

```sh
cmake -S . -B out/portable \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DLIVEKIT_PROTOCOL_ROOT=/path/to/livekit/protocol \
  -DLIVEKIT_SERVER_USE_CURL=ON -DLIVEKIT_SERVER_USE_OPENSSL=ON \
  -DLIVEKIT_SERVER_BUILD_TESTS=ON -DLIVEKIT_SERVER_BUILD_INTEGRATION_TESTS=OFF
cmake --build out/portable --config Release --parallel
ctest --test-dir out/portable -C Release -L unit --output-on-failure
cmake --install out/portable --config Release --prefix /path/to/staging
cmake -S test/consumer -B out/consumer -DCMAKE_PREFIX_PATH=/path/to/staging
cmake --build out/consumer --config Release --parallel
```

Ensure dependency package prefixes are also discoverable for the installed consumer. Dynamic
builds require their dependency libraries on the OS loader path. On Windows, normalize duplicate
PATH/Path keys as described in AGENTS.md if MSBuild fails before compilation.

The libcurl transport reuses connections with synchronized handle access. It restricts protocols
to HTTP/HTTPS, verifies TLS certificates and hostnames, disables redirects to avoid forwarding
credentials to another endpoint, preserves HTTP status/body for Twirp errors, and uses binary
body lengths and request timeouts. Install a valid CA trust store for the selected libcurl TLS
backend; a custom transport can implement application-specific trust or proxy policy.

## Validation still needed on target platforms

Run native unit/C-header/installed-consumer checks, Ruby/Python/.NET/PHP/Rust/Zig offline tests, and Java
offline smoke with `-Xcheck:jni` on Linux and macOS. Then explicitly opt into local-server integration for RPC,
TLS trust and rejection, proxies, timeouts, concurrent requests and webhook verification. Test
both static/shared packaging and loader paths. Windows checks of these backends do not cover
Linux certificate stores, macOS frameworks, compiler/runtime differences or dylib/rpath behavior.

Implementation references: [libcurl easy options](https://curl.se/libcurl/c/curl_easy_setopt.html)
and [OpenSSL EVP MAC](https://docs.openssl.org/3.4/man3/EVP_MAC/).

An optional shared-library loopback probe uses only Python's standard library and a local mock
HTTP server, without a LiveKit server. Build with `BUILD_SHARED_LIBS=ON`, tests/examples OFF,
then run `python test/transport/portable_smoke.py /absolute/path/to/shared/library`. It checks
C exports, binary/Unicode RPC data, HTTP/Twirp error details, redirect rejection, timeout and concurrency.
Dependency DLLs must be on PATH on Windows. It opens loopback sockets and is deliberately not
registered as a deterministic unit test. This probe does not validate TLS trust or proxies.

Windows validation also exercises portable shared C exports, the loopback probe, SHA-256/HMAC
known-answer vectors, and Ruby/JNI offline checks. RubyInstaller may ignore PATH for DLL lookup;
see the [Ruby dependency deployment notes](../pkg/ruby/README.md).
