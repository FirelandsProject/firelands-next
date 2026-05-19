# C++20 migration plan — Firelands Next

This document is the **implementation plan** for raising the project from **C++17** to **C++20**.

**Status:** Phase 1 complete (2026-05-18) — `CMAKE_CXX_STANDARD 20` at root; tool targets use `cxx_std_20`. Phase 2 (incremental feature adoption) open.

**Phase 1 checklist**

- [x] Root `CMakeLists.txt`: `CMAKE_CXX_STANDARD 20`
- [x] All tool `CMakeLists.txt`: `cxx_std_20`
- [x] Developer docs, TechStack skill, `firelands-cpp` cursor rule
- [x] `SPDLOG_USE_STD_FORMAT` (bundled fmt in spdlog 1.14.1 fails on C++20 / Apple Clang)
- [x] Trial build: `auth` + `world` on macOS (Apple Clang, arm64)
- [ ] Trial builds on full compiler matrix (§4.1): Linux GCC/Clang, Windows MSVC
- [x] Phase 2 P1: `std::span` on `ByteBuffer` (`Append`/`Read`/`AsSpan`/`UnreadSpan`)
- [x] Boost.Asio C++20 coroutines across `infrastructure/network/` (see below)
- [ ] Phase 2 remaining: designated initializers, `using enum`

---

## 1. Goals & non-goals

### Goals

| Goal | Rationale |
|------|-----------|
| **Single standard** | One `CMAKE_CXX_STANDARD` for `auth`, `world`, libraries, tests, and tools. |
| **Safer, clearer code** | Adopt C++20 features that reduce bugs and boilerplate without changing architecture. |
| **Maintained portability** | Keep building on Windows (MSVC/MinGW), Linux (GCC/Clang), macOS (Apple Clang). |
| **Incremental rollout** | Flag flip first, feature adoption in follow-up PRs. |

### Non-goals (this milestone)

- **C++23** (`std::expected`, `std::print`, etc.) — separate future plan.
- **Large refactors** “because C++20 allows it” (e.g. rewriting entire `WorldSession` for `coroutines`).
- **Mandatory `std::format` everywhere** — adopt where it clearly wins; keep `spdlog` fmt-style formatting as-is initially.
- **Changing third-party APIs** (MariaDB, Lua, StormLib) — they stay C/C++17-compatible at their boundaries.

---

## 2. Current state (baseline audit)

### Toolchain (CMake)

| Location | Setting |
|----------|---------|
| Root `CMakeLists.txt` | `CMAKE_CXX_STANDARD 17`, `CMAKE_CXX_STANDARD_REQUIRED ON` |
| `tools/extractors/`, `tools/vmap/*`, `tools/config_tui/` | `cxx_std_17` on several targets |

### Documentation & agent guidance

References to **C++17** appear in (non-exhaustive):

- `docs/EN/DEVELOPER_SETUP.md`, `docs/EN/CONTRIBUTING.md`
- `docs/EN/modules/tools-sql-build.md`, `docs/EN/INSPIRATION.md`
- `docs/ES/*` counterparts
- `.agent/skills/TechStack/SKILL.md`, `.opencode/skills/TechStack/SKILL.md`
- `.cursor/rules/firelands-cpp.mdc`
- `README.md`, `AGENTS.md` (implicit via skills)

### Code already using C++17 heavily

Typical patterns in tree today:

- `std::optional`, `std::string_view`, `std::filesystem`
- `std::variant` (where applicable in domain/application)

No dedicated `docs/**` or roadmap item previously tracked C++20.

---

## 3. Why migrate

| Area | C++20 benefit for Firelands |
|------|-----------------------------|
| **Network / packets** | `std::span<const std::byte>` for buffer views; fewer raw pointer + length pairs. |
| **Logging / errors** | `std::source_location` for call-site metadata (optional, behind a thin macro). |
| **Domain / ports** | `concept` for small template constraints in tests and helpers (optional). |
| **Boilerplate** | Designated initializers for plain structs; `using enum` for opcode/enum clarity. |
| **Algorithms** | `std::ranges` / `std::views` for read-only transforms (use sparingly; readability first). |
| **Concurrency** | `std::jthread` + stop tokens where we add new threaded workers (not a mass replace of `std::thread`). |
| **Compile-time** | Broader `constexpr` for constants and packet size checks. |

**Driver:** C++17 is sufficient today, but C++20 is the practical “modern baseline” for new dependencies, compiler defaults, and hiring/onboarding. Migration is **low urgency** until client stability and parity work allow a short toolchain freeze.

---

## 4. Prerequisites

### 4.1 Minimum compiler matrix (target)

After migration, document and CI-enforce (when CI exists):

| Platform | Compiler | Minimum (indicative) | Notes |
|----------|----------|----------------------|-------|
| Linux | GCC | **12** | Full libstdc++ C++20; GCC 11 is partial — avoid as floor. |
| Linux | Clang | **15** | Matches current dev doc floor for C++17. |
| macOS | Apple Clang | **15** (Xcode 15+) | Verify `std::format` / `<ranges>` on oldest supported macOS. |
| Windows | MSVC | **VS 2022 17.4+** | `/std:c++20`; already listed for C++17 in dev setup. |
| Windows | MinGW | **GCC 12+** UCRT | Same as Linux GCC notes. |

**Action before Phase 1:** Each maintainer runs a **trial build** with `CMAKE_CXX_STANDARD 20` locally and records blockers (compiler version, missing headers, dep warnings).

### 4.2 Third-party dependency audit

Pinned via `FetchContent` / `find_package` in root `CMakeLists.txt`:

| Dependency | Version (pinned) | C++20 risk | Mitigation |
|------------|------------------|------------|------------|
| GoogleTest | 1.14.0 | Low | Supports C++20; may emit deprecation warnings — fix or suppress in tests only. |
| spdlog | 1.14.1 | Medium | Build with `SPDLOG_USE_STD_FORMAT` (see root `CMakeLists.txt`); bump spdlog later optional. |
| nlohmann/json | 3.11.2 | Low | Header-only; widely used with C++20. |
| yaml-cpp | 0.8.0 | Low–medium | Build as C++20 TU; watch for `std::string` ABI — same as today. |
| MariaDB C/C++ | 3.3.8 / 1.1.7 | Low | C API boundary unchanged. |
| Lua | 5.4.7 | None | C API. |
| Boost (thread) | system | Low | Boost 1.7x + C++20 is common. |
| StormLib | 9.26 | Low | C-style API; built as C++20 TU. |
| FTXUI | 5.0.0 | Medium | Trial-build `auth`/`world` consoles + `firelands-config`; fix or pin if needed. |

**Deliverable:** A short table in this doc (section 8 checklist) marked pass/fail after trial builds.

### 4.3 CMake

- **Current:** `cmake_minimum_required(VERSION 3.10)`.
- **Recommendation:** Bump to **3.16+** (prefer **3.20+**) in the same PR as C++20 **or** immediately before it, so `target_compile_features(... cxx_std_20)` and generator expressions behave consistently. Coordinate with `.agent/skills/Optimization/SKILL.md`.

---

## 5. Phased migration

### Phase 0 — Inventory & trial (no behavior change)

**Owner:** Any contributor with all three OSes or team split.

- [ ] Trial `set(CMAKE_CXX_STANDARD 20)` on a branch; full `ninja -C build` for: `auth`, `world`, `FirelandsDevTools`, tests (`-DFIRELANDS_BUILD_TESTS=ON`), extractors/vmap tools.
- [ ] Record compiler versions and errors in a PR comment or issue.
- [ ] Grep for C++20-incompatible patterns (rare in C++17 code): `std::result_of`, deprecated `std::iterator`, custom `std::binary_function`, etc.
- [ ] Confirm no `.cpp` file relies on **pre-C++17** extension removed in C++20 (unlikely).

**Exit criteria:** Known blockers documented; deps compile clean or with acceptable warnings list.

---

### Phase 1 — Toolchain flip (mechanical)

**Single PR preferred** — “build as C++20, no feature drive-by”.

1. Root `CMakeLists.txt`: `CMAKE_CXX_STANDARD 20`.
2. Replace all `cxx_std_17` → `cxx_std_20` in tool `CMakeLists.txt` files.
3. Optional: `cmake_minimum_required` bump if required by generators/CI.
4. Fix **compile errors only** (e.g. `char8_t` ambiguities, `operator<=>` conflicts — usually none).
5. Full build all targets; run `ctest` when authorized.

**Docs in same PR:**

- `docs/EN/DEVELOPER_SETUP.md`, `docs/EN/CONTRIBUTING.md`
- `docs/ES/*` mirrors
- `.agent/skills/TechStack/SKILL.md` (+ `.opencode` copy if kept in sync)
- `.cursor/rules/firelands-cpp.mdc`
- `README.md` / `AGENTS.md` one-liners if they mention C++17

**Exit criteria:** Green build on Linux, macOS, Windows; no new warnings policy violation (project default: treat new warnings as fix or justify).

---

### Phase 2 — Adopt high-value C++20 features (incremental PRs)

**Order suggested** (each PR small, TDD where behavior-touching):

| Priority | Feature | Where | Notes |
|----------|---------|-------|-------|
| P1 | `std::span` | `src/shared/network/`, packet read/write helpers | Non-owning views; keep explicit bounds checks at boundaries. |
| P1 | Designated initializers | New structs in `domain/`, config DTOs | Only for aggregate types; match existing style. |
| P2 | `using enum` | Opcode / enum-heavy headers in `shared/network` | Reduces `static_cast` noise; one enum per PR. |
| P2 | `constexpr` expansion | Packet sizes, magic constants | No change to wire format. |
| P3 | `std::ranges` | New code only; optional refactors | Avoid obscure view pipelines. |
| P3 | `std::format` | New string formatting without hot-path allocs | Do not replace spdlog format strings in bulk. |
| P4 | `concept` | Test doubles, small templates in `tests/unit/` | Keep domain headers free of heavy concepts unless clear win. |
| P4 | `std::jthread` | New background tasks only | Do not rewrite existing thread pools in one PR. |

**Coroutines (network I/O — complete for callback-style handlers)**

| Component | Read | Write | Timers / other |
|-----------|------|-------|----------------|
| `AsioAwaitables.h` | `AsyncReadSome` | `AsyncWrite` | `SpawnDetached` |
| `AsyncNetworkServer` | `AcceptLoop` | — | — |
| `AuthSession` | `ReadLoop` | `WriteLoop` (queue) | — |
| `WorldSession` | `ReadLoop` | `WriteLoop` (queue) | `TimeSyncLoop`, deferred spell `co_await` |
| `RestAuthServer` | `async_read` | `async_write` | accept loop |
| `RealmLinkSession` | `ReadLoop` | `SendAck` | — |
| `RealmLinkOutbound` | `async_read` | `AsyncWrite` + ping loop | `steady_timer` |

No completion-handler chains remain in `src/infrastructure/network/` except inside `AsioAwaitables` wrappers (`use_awaitable`).
- **Modules** — CMake/tooling cost too high for this repo today.
- **`<format>` in logging hot paths** — profile first.

**Exit criteria:** At least P1 items landed or consciously deferred with issue links; no regression in client stability checklist (`docs/ES/ROADMAP.md`).

---

### Phase 3 — Cleanup & enforcement

- [ ] Add a **CI job** (when GitHub Actions or equivalent exists) matrix: GCC 12, Clang 15, MSVC 2022, Apple Clang — all `-DCMAKE_CXX_STANDARD=20` (redundant once global).
- [ ] Optional: `cmake/CheckCXX20.cmake` or compile test that `#error`s if `__cplusplus < 202002L`.
- [ ] Update `docs/ES/ROADMAP.md` Phase 1 bullet: “CMake / C++20”.
- [ ] Close migration tracking issue; mark this doc **Status: Completed** with date.

---

## 6. Coding guidelines (post-migration)

1. **Prefer standard library** over platform APIs (unchanged from C++17 mandate).
2. **New code** may use C++20 features listed in Phase 2; **do not** require them in headers consumed by C until Lua/C boundaries are isolated (already the case).
3. **Hexagonal rules unchanged** — no infra headers in `domain/`; concepts do not justify including Asio/MySQL in domain.
4. **Reviews:** Reject PRs that only restyle working C++17 into C++20 without measurable benefit.
5. **PCH:** When adding C++20 headers (`<span>`, `<format>`, `<ranges>`), add to `PROJECT_PCH_HEADERS` only if measured win (see Optimization skill).

---

## 7. Risks & rollback

| Risk | Mitigation |
|------|------------|
| Older macOS / Linux in the wild | Publish minimum Xcode/gcc versions in DEVELOPER_SETUP; fail configure with clear message. |
| FTXUI / niche dep breakage | Pin version or patch fork; isolate to console TUs. |
| Warning explosion | Fix or `-Wno-*` per-target only with comment; no global silence. |
| ABI across DLL boundaries | N/A today (static executables); revisit if shared libs added. |

**Rollback:** Revert Phase 1 PR (CMake + doc flags) — code without C++20-only syntax compiles as C++17 again.

---

## 8. Definition of done

Migration is **complete** when all are true:

1. `CMAKE_CXX_STANDARD` is **20** at root; no `cxx_std_17` left in tree.
2. `auth`, `world`, tests, and agreed tool targets build on the compiler matrix (§4.1).
3. Developer docs and TechStack skill say **C++20**.
4. Phase 2 **P1** items (`span` / designated init) are done **or** tracked with explicit deferrals.
5. Spanish roadmap foundation bullet updated.

---

## 9. Suggested tracking

| Item | Suggestion |
|------|------------|
| GitHub issue | `toolchain: migrate to C++20` with Phase 0–3 checklist |
| Roadmap | Link from `docs/ES/ROADMAP.md` under a “Toolchain” subsection (low priority vs client stability) |
| Priority | **After** Obj 0 client stability; **before** large greenfield subsystems that would benefit from `span`/concepts |

---

## 10. Related documents

- [Developer Setup](DEVELOPER_SETUP.md) — compiler install
- [Contributing](CONTRIBUTING.md) — style and build
- [Tools / SQL / build](modules/tools-sql-build.md) — FetchContent deps
- [StormLib roadmap](STORM_LIB_ROADMAP.md) — extractors (also on C++17 today)
- [Roadmap](ROADMAP.md) — product priorities (SSOT: Spanish roadmap)

---

## Revision history

| Date | Change |
|------|--------|
| 2026-05-18 | Initial plan (project on C++17; no prior migration doc) |
| 2026-05-18 | Phase 1: CMake and docs flipped to C++20 |
