# LiveKit Server C++ SDK Implementation Plan

Last updated: 2026-10-07

## Goals and prioritization

Implementation order is fixed:

1. Complete a cross-platform, stable, releasable SDK for self-hosted LiveKit first.
2. Add LiveKit Cloud-specific services and regional resilience afterward.
3. Every milestone must include the public API, implementation, GTest coverage, installed-package
   consumer tests, and documentation. Adding only protobuf RPC forwarding code is insufficient.

Priorities are determined by:

- Whether self-hosted deployments work directly from Windows, Linux, and macOS backends.
- Whether public headers remain stable and avoid exposing internal generated dependencies.
- Whether requests can be cancelled, timed out, diagnosed, and retried safely.
- Whether functionality matches the public services in the local `server-sdk-go-main` checkout.
- Whether Cloud-only functionality can remain independent of self-hosted releases.

## Current baseline

Implemented:

- All 52 current self-hosted control-plane RPCs in the local protocol checkout: Room and
  Participant administration (14, including `PerformRpc`), Egress (10), Ingress (4), SIP (16),
  Agent Dispatch (3), and WhatsApp/Twilio Connector (5).
- HS256 AccessToken generation.
- Webhook JWT, lifetime, body-digest, multiple-key, and dynamic-key verification with synchronous
  event callbacks and complete `raw_body` preservation.
- WinHTTP transport on Windows, connection reuse, and custom `HttpTransport` injection.
- Protobuf-independent SDK service models with an optional generated-protobuf adapter.
- Standalone compilation for every public header using only the SDK include directory.
- GTest unit tests, opt-in room/webhook real-server integration tests, and installed-package
  consumers with the protobuf adapter both enabled and disabled.

Major gaps:

- Release blocker: Linux and macOS have no default HTTP implementation; only an
  application-supplied transport works there.
- The SDK-owned service models are distinct canonical-JSON containers but do not yet provide typed
  C++ fields, builders, collection accessors, or validation before a service call.
- Calls are synchronous and have only a client-wide timeout. There is no per-request timeout,
  cancellation, caller headers, caller request ID, asynchronous API, or retry policy.
- Transport errors do not yet distinguish DNS, connect, TLS, send, receive, timeout, and
  cancellation, and WinHTTP responses are not bounded by a configurable maximum size.
- AccessToken is missing current participant permissions, participant kind/details, room
  configuration/preset/agents, SHA-256, Inference, and Observability claims.
- Go SDK parity helpers are missing: RoomService token creation, ordered SIP batch lookups,
  structured SIP status extraction, request validation, and ringing-aware SIP/WhatsApp timeouts.
- `SendData` nonce behavior and retry/idempotency semantics are not yet fully matched and tested
  against the Go SDK.
- Real-server integration coverage is limited to room create/delete and a `room_started` webhook;
  other services and participant/track webhook events are not covered.
- There is no three-platform CI matrix, DLL export policy, ABI check, sanitizer coverage, or full
  installed shared/static consumer matrix.
- The 30 Cloud Phone Number, Cloud Agent, and Agent Simulation RPCs, Cloud Agent operational
  helpers, and LiveKit Cloud region discovery/failover are absent.

Execution order from the current baseline:

1. S1: add and harden the cross-platform transport.
2. S2: add per-request control, cancellation, diagnostics, and reliability semantics.
3. S3: complete tokens, typed model ergonomics, helpers, validation, and webhook coverage.
4. S4: pass the three-platform release gate and publish the first self-hosted release.
5. C1-C4: add Cloud services first, then regional resilience and advanced grants.

## Phase S0: Stabilize the public API boundary

Status: completed on 2026-10-07. The protobuf-independent service models, conversion layer,
optional compatibility target, public-header compile test, and installed-consumer fixtures are in
place. Later phases may add typed convenience accessors without changing the model boundary.

Goal: establish a maintainable API and ABI for the self-hosted release before further service
expansion causes repeated breaking changes.

Implementation:

- Add SDK-owned request, response, and domain models under a directory such as
  `include/livekit/server/model/`.
- Stop requiring generated protobuf types as parameters or return values in primary public service
  interfaces.
- Implement bidirectional SDK model/protobuf conversion under `src/detail/proto/`.
- Keep an explicit, optional protobuf adapter layer as a migration path for existing consumers.
- Define static and shared library dependency policies:
  - Public source code must not require protobuf include paths.
  - CMake or the package manager must resolve binary link dependencies automatically.
  - Do not merge protobuf static objects into the SDK because that risks duplicate symbols and ABI
    conflicts in host applications.
- Add compile tests for every public header with only the SDK `include/` directory available.
- Define unknown-value behavior for public enums and errors to preserve forward compatibility.

Acceptance criteria:

- No `*.pb.h` or protobuf include exists under `include/livekit/server/`.
- A consumer without generated protocol headers can use AccessToken, Webhook, and SDK-owned service
  models.
- Enabling and disabling the protobuf adapter are covered by separate build tests.
- Existing APIs receive compatible overloads, adapters, or explicit major-version migration notes
  when a breaking change cannot be avoided.

## Phase S1: Cross-platform self-hosted foundation

Status: next implementation phase and the primary blocker for a self-hosted release.

Goal: call a self-hosted LiveKit Server directly from Windows, Linux, and macOS.

Implementation:

- Retain the WinHTTP backend.
- Add a libcurl-based default `HttpTransport` for non-Windows platforms with HTTP/HTTPS, system
  proxy support, CA validation, connection reuse, and response-size limits.
- Add `USE_SYSTEM_CURL` or an equivalent option and pin any vendored release with a checksum.
- Normalize URL, IPv4/IPv6, proxy, TLS, timeout, and error-mapping behavior across backends.
- Report clear error context for connection, request send, and response read failures.
- Populate response headers consistently on every backend.
- Add configurable request and response body limits, and check ranges before every narrowing
  conversion, including WinHTTP `DWORD` conversions.
- Test malformed URLs, proxy failures, TLS failures, redirects, truncated bodies, oversized bodies,
  IPv6 literals, and UTF-8 host/path/header handling without accessing physical media devices.

Acceptance criteria:

- Windows builds and tests with WinHTTP; Linux and macOS build and test with libcurl.
- CreateRoom/ListRooms/DeleteRoom integration tests pass on all three platforms.
- TLS verification is enabled by default and never silently downgraded.
- Transport failures identify the failed stage and retain the underlying platform error context.
- Oversized input fails before an unsafe narrowing conversion or unbounded allocation.
- Applications can still replace the default implementation with a custom `HttpTransport`.

## Phase S2: Request control and reliability

Status: planned after S1.

Goal: make the SDK suitable for long-running services rather than synchronous examples only.

Implementation:

- Add `RequestOptions` with:
  - Per-request timeout.
  - `std::stop_token` cancellation.
  - Additional HTTP headers.
  - A caller-provided or SDK-generated request ID.
- Add compatible overloads accepting `RequestOptions` to every service method.
- Reuse `X-Livekit-Request-Id` across retries of one logical request.
- Distinguish connection failure, timeout, cancellation, HTTP, Twirp, and protocol parsing errors.
- Preserve HTTP response headers and relevant transport error details without including credentials,
  tokens, or secret response bodies in exception messages.
- Define which operations may be retried automatically. Default retries must be limited to safe or
  explicitly idempotent calls; mutating calls require a stable request ID and a documented server
  deduplication contract.
- Add thread-safety and concurrent-call tests.
- Add asynchronous APIs only after the synchronous API is stable. Prefer cancellable C++20
  future/executor integration first, with coroutines as an optional layer. The library must not
  create uncontrolled detached threads.

Acceptance criteria:

- Cancellation interrupts DNS, connect, send, and receive operations without waiting for the full
  default timeout.
- A call can override the global timeout without changing concurrent calls.
- Custom headers cannot override security-sensitive headers unless explicitly documented.
- Cancellation and timeout have distinct stable `ErrorCode` values and preserve useful diagnostic
  context.
- Retries never regenerate the request ID, signed token, request body, or caller-supplied nonce.
- Concurrent tests are race-free on platforms where TSAN is available.

## Phase S3: Self-hosted authentication and core-service completeness

Status: planned after request control is stable. The protocol RPC inventory is complete; this phase
focuses on token parity, typed ergonomics, validation, and Go SDK behavioral parity.

Goal: complete token claims, convenience APIs, and protocol compatibility required by self-hosted
deployments.

Implementation:

- Add typed C++ builders and read-only accessors for the commonly used SDK-owned Room, Participant,
  Egress, Ingress, SIP, Agent Dispatch, and Connector models. Retain canonical JSON as the
  forward-compatible escape hatch and do not expose protobuf types from the primary API.
- Add these AccessToken claims:
  - `canSubscribeMetrics`.
  - `canManageAgentSession`.
  - Participant kind and kind detail.
  - Room preset, room configuration, and room agent dispatch.
- Add `SetSha256` parity and room-configuration sensitive-credential checks. Tokens containing
  storage or service credentials must be rejected by default unless a clearly named server-only
  override is enabled.
- Add a RoomService convenience method that creates an AccessToken from the current API key and
  secret. Pre-signed-token mode must return a clear cannot-sign error.
- Add ordered SIP batch lookup helpers for trunks and dispatch rules.
- Add structured conversion for SIP call status and Twirp/HTTP errors.
- Match Go SDK request validation for SIP create/update/list/delete operations and return
  `invalid_argument` before sending malformed requests.
- Add ringing-aware timeout behavior for SIP participant creation, SIP transfer, and WhatsApp calls
  that wait for an answer, while respecting explicit caller cancellation.
- Audit every current Room, Egress, Ingress, SIP, Agent Dispatch, and Connector RPC. When the
  protocol adds a core RPC, update forward declarations, implementation, and route/grant tests
  together.
- Match the Go SDK's automatic SendData nonce behavior and verify idempotency semantics.
- Add webhook convenience fields for all current events and unknown-field regression tests while
  continuing to preserve the complete `raw_body`. Multiple signing keys and dynamic key lookup are
  already implemented and must remain covered.

Acceptance criteria:

- Every self-hosted core route, grant, serialization path, and error path has GTest coverage.
- AccessToken claims match the Go SDK's semantics for identical fixed inputs.
- Typed model users can perform common self-hosted workflows without authoring or parsing JSON.
- Invalid SIP inputs fail locally, and calls that wait for an answer are not cut off by the normal
  short request timeout.
- Real local-server webhook tests cover at least room, participant, and track events.

## Phase S4: Self-hosted release gate

Status: planned after S1-S3. Cloud work must not begin until this gate passes.

Goal: produce the first stable self-hosted release before beginning Cloud expansion.

Implementation:

- Cover Windows, Linux, and macOS in CI, including at least Debug/Release and shared/static consumer
  combinations.
- Add explicit symbol visibility/export annotations and verify that a Windows shared-library
  consumer links and runs; do not rely on static-library-only success.
- Complete install exports, version compatibility files, and pkg-config or an equivalent consumer
  workflow.
- Add ABI/API checks and standalone public-header compilation.
- Run ASan/UBSan where supported and TSAN on at least one platform for concurrent client, transport,
  token, and webhook paths.
- Complete service examples, a webhook HTTP-framework integration example, error-handling guidance,
  cancellation guidance, and threading documentation.
- Perform a pre-release security review of credentials, log redaction, TLS, webhook verification,
  and the dependency supply chain.
- Run the complete self-hosted integration matrix using the local LiveKit Server and CLI.

Cloud-phase entry criteria:

- No known blocking defect remains in self-hosted core interfaces.
- Installed consumers pass on all three platforms.
- Static and shared installed consumers pass with the protobuf adapter both enabled and disabled.
- Unit and explicit integration tests are stable and contain no timing-dependent flaky cases.
- The public API and ABI policy is finalized and documented.

## Phase C1: Cloud Phone Number

Status: deferred until the self-hosted release gate passes.

Goal: add the six LiveKit Cloud phone-number management methods.

Methods:

- `SearchPhoneNumbers`
- `PurchasePhoneNumber`
- `ListPhoneNumbers`
- `GetPhoneNumber`
- `UpdatePhoneNumber`
- `ReleasePhoneNumbers`

Implementation requirements:

- Add a standalone `PhoneNumberClient` and evaluate whether it should also be exposed through a
  `LiveKitApi` accessor.
- Use the SIP admin grant.
- Give search a longer default timeout while retaining caller timeout overrides and cancellation.
- Cloud-only builds and tests must not affect self-hosted-only consumers.

## Phase C2: Cloud Agent management

Status: deferred until C1 and the self-hosted release gate pass.

Goal: add the 18 Cloud Agent methods.

Method groups:

- Lifecycle: `CreateAgent`, `CreateAgentV2`, `UpdateAgent`, `DeleteAgent`, `RestartAgent`, and
  `RollbackAgent`.
- Deployment: `DeployAgent`, `DeployAgentV2`, and `PromoteAgent`.
- Queries: `ListAgents`, `ListAgentVersions`, and `GetClientSettings`.
- Secrets: `ListAgentSecrets` and `UpdateAgentSecrets`.
- Private Link: `CreatePrivateLink`, `DestroyPrivateLink`, `ListPrivateLinks`, and
  `GetPrivateLinkStatus`.

Implementation requirements:

- Support derived and explicitly overridden Cloud Agent endpoints without putting Cloud hostname
  rules into the generic transport.
- Use the current protocol's Agent admin grant for management calls. Do not invent grant fields
  that are absent from the pinned protocol; add new fields only when the local protocol and Go SDK
  expose them.
- Never write secret request or response contents to normal logs or full exception messages.
- After RPC parity, evaluate Go SDK Cloud Agent operational helpers separately: source upload and
  build, deployment log streaming, registry/push target handling, and explicit region selection.
- Provide streaming request/response extension points for uploads and logs rather than forcing
  large or unbounded payloads through the in-memory protobuf call path.

## Phase C3: Agent Simulation

Status: deferred until C2.

Goal: add the six Agent Simulation methods.

Methods:

- `CreateSimulationRun`
- `ConfirmSimulationSourceUpload`
- `GetSimulationRun`
- `ListSimulationRuns`
- `CancelSimulationRun`
- `CreateScenarioFromSession`

Implementation requirements:

- Use the `simulationAdmin` grant.
- Test expired upload URLs, repeated confirmation, and cancellation.
- Provide a streaming extension point when large requests or uploads should bypass the normal
  in-memory protobuf body path.

## Phase C4: Cloud regional resilience and advanced grants

Status: deferred until the Cloud service clients are available.

Goal: match the Go SDK's Cloud reliability and newer service authorization capabilities.

Implementation:

- Add `/settings/regions` discovery, caching, and refresh.
- Use at most three attempts by default, exponential backoff, and an independent timeout budget per
  attempt.
- Enable failover by default only for LiveKit Cloud hosts. Do not guess cross-region endpoints for
  self-hosted deployments.
- Do not retry 4xx responses. Retry network failures and 5xx responses according to policy while
  preserving the request ID and body.
- Allow failover to be disabled explicitly and permit tests to inject region lists and a backoff
  clock.
- Add Inference and Observability AccessToken grants matching the pinned protocol. Keep grant
  serialization tests independent from Cloud network tests.

Acceptance criteria:

- Deterministic tests cover the primary region failing, two regions failing, all regions failing,
  4xx, 5xx, timeout, and cancellation.
- Non-Cloud URLs never trigger region discovery.
- Retries never regenerate the token, nonce, or request ID.

## Testing and commit strategy

Split each phase into independently reviewable commits:

1. Protocol models/forward declarations and CMake.
2. Client implementation and grant/route behavior.
3. GTest unit coverage.
4. Opt-in integration tests and examples.
5. Install exports and documentation.

Run at least these checks before every commit:

```powershell
git diff --check
cmake --build <build-dir> --config Release --parallel
ctest --test-dir <build-dir> -C Release -L unit --output-on-failure
```

Changes to the public boundary or packaging must additionally build installed consumers in two
fresh prefixes:

- `LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER=OFF`: no generated `*.pb.h` file or adapter target is
  installed, and the SDK-model consumer builds.
- `LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER=ON`: generated headers stay under the isolated adapter
  include directory, and the protobuf compatibility consumer builds.

Transport changes must run backend unit tests without network access, followed by the explicit
local-server integration label on each supported platform. Cancellation, timeout, retry, and
failover tests must use injected transports/clocks and remain deterministic.

For real-service behavior, run the `integration` label against the local LiveKit Server only after
explicit authorization. Cloud phases must use a dedicated test project and short-lived credentials;
never place real credentials in command logs or the repository.
