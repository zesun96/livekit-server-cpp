# LiveKit Server C++ SDK Implementation Plan

Last updated: 2026-09-02

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

- Room and Participant administration, including `PerformRpc`.
- Egress, Ingress, SIP, Agent Dispatch, and WhatsApp/Twilio Connector services.
- HS256 AccessToken generation.
- Webhook JWT, lifetime, and body-digest verification with event callbacks.
- WinHTTP transport on Windows, connection reuse, and custom `HttpTransport` injection.
- GTest unit tests, opt-in real-server integration tests, and installed-package consumer tests.

Major gaps:

- No default HTTP implementation on non-Windows platforms.
- Public service signatures still use protobuf types even though public headers do not directly
  include generated headers.
- No per-request cancellation, timeout, extra headers, or asynchronous API.
- AccessToken does not cover all current grants and room configuration claims.
- The 30 Cloud Phone Number, Cloud Agent, and Agent Simulation methods are absent.
- LiveKit Cloud region discovery and failover are absent.

## Phase S0: Stabilize the public API boundary

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

Goal: call a self-hosted LiveKit Server directly from Windows, Linux, and macOS.

Implementation:

- Retain the WinHTTP backend.
- Add a libcurl-based default `HttpTransport` for non-Windows platforms with HTTP/HTTPS, system
  proxy support, CA validation, connection reuse, and response-size limits.
- Add `USE_SYSTEM_CURL` or an equivalent option and pin any vendored release with a checksum.
- Normalize URL, IPv4/IPv6, proxy, TLS, timeout, and error-mapping behavior across backends.
- Report clear error context for connection, request send, and response read failures.
- Limit request and response body sizes and check ranges before every narrowing conversion.

Acceptance criteria:

- Windows builds and tests with WinHTTP; Linux and macOS build and test with libcurl.
- CreateRoom/ListRooms/DeleteRoom integration tests pass on all three platforms.
- TLS verification is enabled by default and never silently downgraded.
- Applications can still replace the default implementation with a custom `HttpTransport`.

## Phase S2: Request control and reliability

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
- Add thread-safety and concurrent-call tests.
- Add asynchronous APIs only after the synchronous API is stable. Prefer cancellable C++20
  future/executor integration first, with coroutines as an optional layer. The library must not
  create uncontrolled detached threads.

Acceptance criteria:

- Cancellation interrupts DNS, connect, send, and receive operations without waiting for the full
  default timeout.
- A call can override the global timeout without changing concurrent calls.
- Custom headers cannot override security-sensitive headers unless explicitly documented.
- Concurrent tests are race-free on platforms where TSAN is available.

## Phase S3: Self-hosted authentication and core-service completeness

Goal: complete token claims, convenience APIs, and protocol compatibility required by self-hosted
deployments.

Implementation:

- Add these AccessToken claims:
  - `canSubscribeMetrics`.
  - `canManageAgentSession`.
  - Participant kind and kind detail.
  - Room preset, room configuration, and room agent dispatch.
- Add a RoomService convenience method that creates an AccessToken from the current API key and
  secret. Pre-signed-token mode must return a clear cannot-sign error.
- Add ordered SIP batch lookup helpers for trunks and dispatch rules.
- Add structured conversion for SIP call status and Twirp/HTTP errors.
- Audit every current Room, Egress, Ingress, SIP, Agent Dispatch, and Connector RPC. When the
  protocol adds a core RPC, update forward declarations, implementation, and route/grant tests
  together.
- Match the Go SDK's automatic SendData nonce behavior and verify idempotency semantics.
- Add webhook signing-key rotation, convenience fields for all events, and unknown-field regression
  tests while continuing to preserve the complete `raw_body`.

Acceptance criteria:

- Every self-hosted core route, grant, serialization path, and error path has GTest coverage.
- AccessToken claims match the Go SDK's semantics for identical fixed inputs.
- Real local-server webhook tests cover at least room, participant, and track events.

## Phase S4: Self-hosted release gate

Goal: produce the first stable self-hosted release before beginning Cloud expansion.

Implementation:

- Cover Windows, Linux, and macOS in CI, including at least Debug/Release and shared/static consumer
  combinations.
- Complete install exports, version compatibility files, and pkg-config or an equivalent consumer
  workflow.
- Add ABI/API checks and standalone public-header compilation.
- Complete service examples, a webhook HTTP-framework integration example, error-handling guidance,
  and threading documentation.
- Perform a pre-release security review of credentials, log redaction, TLS, webhook verification,
  and the dependency supply chain.
- Run the complete self-hosted integration matrix using the local LiveKit Server and CLI.

Cloud-phase entry criteria:

- No known blocking defect remains in self-hosted core interfaces.
- Installed consumers pass on all three platforms.
- Unit and explicit integration tests are stable and contain no timing-dependent flaky cases.
- The public API and ABI policy is finalized and documented.

## Phase C1: Cloud Phone Number

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
- Add agent `databaseAdmin` and complete the Cloud Agent admin grant behavior.
- Never write secret request or response contents to normal logs or full exception messages.

## Phase C3: Agent Simulation

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
- Add Inference, Observability, and other Cloud-only AccessToken grants.

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

For real-service behavior, run the `integration` label against the local LiveKit Server only after
explicit authorization. Cloud phases must use a dedicated test project and short-lived credentials;
never place real credentials in command logs or the repository.
