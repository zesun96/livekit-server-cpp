# Repository Guidelines

## Scope and priorities

- These instructions apply to the entire `livekit-server-cpp` repository.
- Preserve public API and ABI compatibility unless the task explicitly requires a breaking
  change. Prefer compatibility overloads or wrappers when correcting an existing API.
- Keep changes focused. Do not mix dependency updates, broad formatting, refactors, and behavior
  changes unless they are required by the same task.
- Inspect the working tree before editing. Existing changes belong to the user and must not be
  overwritten, reverted, or included in a commit accidentally.

## Repository layout

- Public headers: `include/livekit/server/`
- Library implementation: `src/`
- CMake helpers: `cmake/`
- Examples: `examples/`
- Tests:
  - `test/unit/`: deterministic tests with no LiveKit server or network dependency.
  - `test/integration/`: opt-in tests against a real LiveKit server.
  - `test/consumer/`: installed-package and public-header consumption checks.

## C++ rules

- Use C++20 and portable standard-library facilities where practical.
- Format changed C/C++ files with the repository `.clang-format`. Do not reformat unrelated lines
  or entire files merely to satisfy personal style preferences.
- The checked-in style uses tabs for C/C++ indentation, a 100-column limit, left-aligned pointers
  and references, and case-sensitive include sorting.
- Follow `.editorconfig`: UTF-8, LF endings, trim trailing whitespace, and end files with a newline.
- Prefer RAII and value semantics. Use `std::unique_ptr` for sole ownership and `std::shared_ptr`
  only when lifetime is genuinely shared. Raw pointers should normally be non-owning.
- Avoid detached threads. Thread-owning types must define deterministic stop/join behavior and must
  not let callbacks outlive objects they reference.
- Synchronize shared mutable state explicitly. Do not invoke external callbacks while holding an
  internal mutex unless lock ordering and re-entrancy are documented.
- Use scoped enums, `nullptr`, `override`, `const`, and `noexcept` where they clarify contracts.
- Throw exception types by value. Use `livekit::server::Error` for public SDK failures and standard
  exceptions for internal invariant or platform failures where appropriate.
- Validate externally supplied data, credentials, URLs, JWTs, HTTP responses, and protobuf payloads
  before use. Check sizes before converting them to narrower platform API types.
- Keep public headers self-contained. Do not include generated `*.pb.h` or protobuf headers from
  `include/livekit/server/`; use forward declarations and keep generated includes in `.cpp` files.
- Webhook public types must remain protobuf-independent. Preserve the verified raw payload when a
  convenience model cannot represent newer protocol fields.

## CMake and dependencies

- Use target-based CMake (`target_link_libraries`, `target_include_directories`, and
  `target_compile_features`). Avoid global include or link directories and compiler flags.
- Format `CMakeLists.txt` and `*.cmake` with two-space indentation.
- Prefer versioned source archives declared through `FetchContent` and pinned with a cryptographic
  `URL_HASH`. Do not use unpinned branches or full-history clones for dependencies.
- Preserve `USE_SYSTEM_GTEST` and other system-dependency options when changing dependencies.
- Keep the LiveKit Protocol archive revision aligned with the RPC schemas implemented by this SDK.
- Use `LIVEKIT_PROTOCOL_ROOT` for an existing local protocol checkout. Prefer
  `../livekit-client-cpp/protocol` in this workspace instead of downloading another copy.
- Keep protobuf compatible with the selected vcpkg triplet and MSVC runtime. Applications, this
  SDK, GoogleTest, and protobuf must use a consistent `/MD` or `/MT` runtime.
- Never commit downloaded archives, `_deps`, generated build trees, install trees, vcpkg package
  trees, logs, credentials, or generated protobuf files.

## Testing and validation

- Add or update tests for behavior changes and regressions.
- Use GoogleTest for unit and integration tests. Register cases with `gtest_discover_tests`, retain
  the `unit` or `integration` CTest label, and use `GTEST_SKIP()` when opt-in prerequisites are
  absent.
- Unit tests must be deterministic, fast, and independent of the network, wall-clock timing where
  avoidable, and external services.
- Integration tests must remain behind `LIVEKIT_SERVER_BUILD_INTEGRATION_TESTS=ON`. Read connection
  information from `LIVEKIT_URL` and credentials from `LIVEKIT_TOKEN` or
  `LIVEKIT_API_KEY`/`LIVEKIT_API_SECRET`; never embed real credentials in source or logs.
- Reuse the local LiveKit server at `../others/livekit-server/bin/livekit-server.exe` and CLI at
  `../others/lk_2.18.2_windows_amd64/lk.exe` before downloading or rebuilding replacements.
- Do not run integration tests merely because credentials or a server are available. Run them only
  when the task explicitly requests real-server validation.
- Before committing, run the smallest relevant checks and expand them in proportion to risk:

  ```powershell
  git diff --check
  cmake --build <build-dir> --config Release --parallel
  ctest --test-dir <build-dir> -C Release -L unit --output-on-failure
  ```

- Validate install/export changes with the installed consumer under `test/consumer/`.
- Run integration tests only when explicitly requested:

  ```powershell
  ctest --test-dir <build-dir> -C Release -L integration --output-on-failure
  ```

- If a full build cannot run because protobuf, vcpkg packages, credentials, network access, or a
  LiveKit server is unavailable, still run relevant syntax/static checks and report the exact gap.

## Windows build environment

- If MSBuild reports duplicate `PATH`/`Path` dictionary keys, normalize them only in the current
  PowerShell process before running every related build command:

  ```powershell
  $normalizedPath = $env:Path
  Remove-Item Env:PATH -ErrorAction SilentlyContinue
  $env:Path = $normalizedPath
  cmake --build <build-directory> --config Release --parallel
  ```

- Do not use `setx`, edit the registry, delete the build tree, or change source code for this tool
  process environment issue.

## Commit rules

- Review `git status --short`, `git diff`, and `git diff --cached` before every commit.
- Stage explicit paths. Never stage build output, install output, generated protocol files,
  credentials, editor state, or unrelated user changes.
- Use Conventional Commit subjects:

  ```text
  <type>(optional-scope): concise imperative summary
  ```

- Common types are `feat`, `fix`, `refactor`, `test`, `build`, `docs`, and `chore`.
- Keep the subject lowercase where natural, without a trailing period, and preferably at or below
  72 characters. Do not commit WIP changes.
- Each commit must represent one reviewable logical change. Explain important motivation,
  compatibility concerns, and validation in the body when the subject is insufficient.
- Run `git diff --cached --check` immediately before committing and inspect the staged file list.
- Do not amend, rebase, squash, force-push, or push commits unless the user explicitly requests it.
- After committing, report the commit hash, validation performed, validation not performed, and any
  remaining working-tree changes.
