# Implementation Plan: MapService Isolation + Operational Improvements

## Overview

Borrow proven patterns from the legacy map host architecture to improve Firelands Next:
1. **MapService isolation** — each map as an observable, independently-ticked service with snapshots
2. **Graceful shutdown** — session draining with timeout before hard stop
3. **Fail-fast config validation** — validate all settings at startup, not at first use
4. **Map status reporting via realm-link** — optional, extends Phase 1 data to auth server

---

## Phase 1: MapService Wrapper + Snapshots (Core Feature)

### 1.1 Create `MapSnapshot` value object

**Location:** `src/domain/world/MapSnapshot.h`

Immutable POD struct:

```cpp
struct MapSnapshot {
    uint32 mapId;
    int playerCount;
    int creatureCount;
  int loadedGridCells; // cells with at least 1 object
  double avgTickTimeMs; // rolling average of TickAuras duration
  double lastTickTimeMs; // last individual tick duration
  bool isEmpty; // no players, no creatures
};
```

- No mutex needed (value type)
- Tests: `tests/unit/domain/MapSnapshotTests.cpp`

### 1.2 Add tick timing instrumentation to `Map`

**Location:** `src/domain/world/Map.h` / `.cpp`

Add to `Map`:

```cpp
void RecordTickTime(double ms);
MapSnapshot CreateSnapshot() const; // thread-safe, takes lock
```

- Internally track exponential moving average (alpha=0.1) of tick times
- `CreateSnapshot()` gathers player count, creature count, occupied grid cells, tick stats under `m_mapMutex`

### 1.3 Create `MapService` wrapper

**Location:** `src/application/services/MapService.h` / `.cpp`

Supervisory wrapper — NOT a replacement for `Map`:

```cpp
class MapService {
public:
    explicit MapService(uint32 mapId, std::shared_ptr<Map> map);

    uint32 MapId() const;
    Map const* GetMap() const;
    Map* GetMap();
    MapSnapshot Snapshot() const;
    void RecordTick(double tickMs);

private:
    uint32 m_mapId;
    std::shared_ptr<Map> m_map;
    mutable std::mutex m_snapshotMutex;
    double m_avgTickTimeMs = 0.0;
    double m_lastTickTimeMs = 0.0;
};
```

- No dependencies on infrastructure
- Tests: `tests/unit/application/MapServiceTests.cpp`

### 1.4 Create `MapRegistry` service

**Location:** `src/application/services/MapRegistry.h` / `.cpp`

Replaces the raw `unordered_map<uint32, shared_ptr<Map>>` in `WorldService`:

```cpp
class MapRegistry {
public:
    std::shared_ptr<MapService> GetOrCreate(uint32 mapId);
    void ForEach(std::function<void(MapService&)> fn);
    std::vector<MapSnapshot> AllSnapshots() const;
    void Clear();

private:
    std::mutex m_mutex;
    std::unordered_map<uint32, std::shared_ptr<MapService>> m_services;
};
```

- Tests: `tests/unit/application/MapRegistryTests.cpp`

### 1.5 Refactor `WorldService` to use `MapRegistry`

**Location:** `src/application/services/WorldService.h` / `.cpp`

Changes:

- Replace `m_maps` with `MapRegistry m_mapRegistry`
- `GetMap(mapId)` delegates to `m_mapRegistry.GetOrCreate(mapId)->GetMap()`
- `AddPlayerToMap`, `RemovePlayerFromMap`, etc. — same logic, through registry
- `ForEachMap(fn)` delegates to `m_mapRegistry.ForEach(...)`
- Add `GetMapSnapshots() -> std::vector<MapSnapshot>` for external consumption
- `ResetForShutdown()` calls `m_mapRegistry.Clear()`
- Update existing tests: `tests/unit/application/WorldServiceTests.cpp`

### 1.6 Update `MapAuraTicker` to record tick times

**Location:** `src/infrastructure/world/MapAuraTicker.h` / `.cpp`

```cpp
void TickMapAuras(std::chrono::steady_clock::time_point now) {
    WorldService::Instance().ForEachMapService([&](MapService& svc) {
        auto start = std::chrono::steady_clock::now();
        TickMap(svc.MapId(), *svc.GetMap(), now);
        auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        svc.RecordTick(elapsed);
    });
}
```

- Tests: `tests/unit/infrastructure/MapAuraTickerTests.cpp`

### 1.7 Expose snapshots in FTXUI dashboard

**Location:** `src/world/WorldFtxuiConsole.cpp`

Add a "Map Status" panel:

```
Map Status:
  Eastern Kingdoms (0) Players: 3 Tick: 2.1ms Grids: 12
  Kalimdor (1) Players: 1 Tick: 1.4ms Grids: 5
  Outland (530) Players: 0 Tick: 0.0ms Grids: 0 [empty]
```

---

## Phase 2: Graceful Shutdown with Session Draining

### 2.1 Add session tracking to `AsyncNetworkServer`

**Location:** `src/infrastructure/network/asio/AsyncNetworkServer.h` / `.cpp`

Add:

```cpp
std::unordered_map<uint64, std::weak_ptr<IAuthSession>> m_sessions;
std::mutex m_sessionMutex;
std::condition_variable m_drainCV;

void RegisterSession(uint64 guid, std::shared_ptr<IAuthSession> session);
void UnregisterSession(uint64 guid);
int ActiveSessionCount() const;
```

- Sessions register on creation, unregister on close
- Uses `weak_ptr` so destroyed sessions auto-clean

### 2.2 Add `StopGraceful(timeout)` method

**Location:** `src/infrastructure/network/asio/AsyncNetworkServer.h` / `.cpp`

```cpp
void StopGraceful(std::chrono::milliseconds timeout);
```

Behavior:

1. Set `_running = false`, close acceptor (no new connections)
2. Iterate all sessions, call `session->RequestDisconnect("Server shutting down")`
3. Wait up to `timeout` for sessions to close (poll `ActiveSessionCount()`)
4. If timeout expires, force `_ioContext.stop()`
5. If all sessions drain cleanly, call `_ioContext.stop()` after

### 2.3 Update `WorldApplication` shutdown sequence

**Location:** `src/world/WorldApplication.cpp`

New shutdown order:

```
1. stopRealmLink.store(true)
2. worldServer->StopGraceful(5s) // NEW: drain sessions
3. WorldService::Instance().ResetForShutdown()
4. realmLinkThread->join()
5. Logger::Shutdown()
```

### 2.4 Add `RequestDisconnect` to AuthSession

**Location:** `src/infrastructure/network/sessions/AuthSession.h` / `.cpp`

Already exists on `WorldSession` via `IAuthSession` port. Ensure `AuthSession` also implements it for the auth server.

### Tests

- `tests/unit/infrastructure/AsyncNetworkServerTests.cpp` (new or extended)

---

## Phase 3: Fail-Fast Config Validation

### 3.1 Create `ConfigValidator`

**Location:** `src/shared/ConfigValidator.h` / `.cpp`

```cpp
class ConfigValidator {
public:
    struct Rule {
  std::string path; // e.g. "Database.Auth.URI"
        std::string description;
        std::function<bool(std::string_view)> validate;
        std::string errorMessage;
};

    static void Validate(std::vector<Rule> rules);
    // Throws std::runtime_error with all accumulated errors
};
```

### 3.2 Define validation rules per server

**Location:** `src/auth/AuthApplication.cpp` and `src/world/WorldApplication.cpp`

Auth server rules:

| Rule | Validation | Severity |
|------|-----------|----------|
| `Database.User` | non-empty | Error |
| `Database.Auth.URI` | non-empty, valid JDBC format | Error |
| `Network.Port` | 1–65535 | Error |
| `RealmLink.Token` | non-empty (if realm-link enabled) | Error |

World server rules:

| Rule | Validation | Severity |
|------|-----------|----------|
| `Database.User` | non-empty | Error |
| `Database.Auth.URI` | non-empty, valid JDBC | Error |
| `Database.Characters.URI` | non-empty, valid JDBC | Error |
| `Database.World.URI` | non-empty, valid JDBC | Error |
| `Network.Port` | 1–65535 | Error |
| `Network.TimeSyncPeriodMs` | > 0 | Error |
| `Data.DbcPath` | non-empty | Warning |
| `Scripting.ScriptsDirectory` | directory exists | Warning |

### 3.3 Call validation at startup

**Location:** Both `AuthApplication.cpp` and `WorldApplication.cpp`

Insert after config load, before any DB connections:

```cpp
ConfigValidator::Validate(GetAuthConfigRules());
// or
ConfigValidator::Validate(GetWorldConfigRules());
```

If validation fails: log all errors, exit with code 1.

### Tests

- `tests/unit/shared/ConfigValidatorTests.cpp`

---

## Phase 4: Map Status Reporting via Realm-Link (Optional)

### 4.1 Extend realm-link protocol

**Location:** `src/infrastructure/network/realm_link/RealmLinkProtocol.h`

Add new packet type: `MAP_STATUS_REPORT`

- Sent from world to auth every 15 seconds
- Contains: mapId, playerCount, avgTickTimeMs for each active map

### 4.2 Auth-side aggregation

**Location:** `src/infrastructure/network/realm_link/RealmLiveRegistry.h`

- Store map status per realm
- Expose via `GetRealmMapStatus(realmId)`

### 4.3 Expose in auth server TUI

**Location:** `src/auth/AuthFtxuiConsole.cpp`

Show per-realm map status in the auth server dashboard.

### Tests

- `tests/unit/shared/RealmLinkProtocolTests.cpp`

---

## Dependency Graph & Execution Order

```
Phase 1 (MapService) ──────────────────────────────────────┐
  1.1 MapSnapshot (domain, no deps) │
  1.2 Map tick timing (domain, no deps) │ Can be
  1.3 MapService (application, depends on 1.1, 1.2) │ done in
  1.4 MapRegistry (application, depends on 1.3) │ parallel
  1.5 WorldService refactor (application, depends on 1.4) │ with
  1.6 MapAuraTicker update (infrastructure, depends on 1.5) │ Phase 2
  1.7 FTXUI dashboard (world exe, depends on 1.6) ─┘

Phase 2 (Graceful Shutdown) ───────────────────────────────┐
  2.1 Session tracking (infrastructure) │
  2.2 StopGraceful (infrastructure, depends on 2.1) │
  2.3 WorldApplication shutdown (world exe, depends on 2.2) │
  2.4 AuthSession disconnect (infrastructure) ─┘

Phase 3 (Config Validation) ───────────────────────────────┐
  3.1 ConfigValidator (shared, no deps) │ Can be
  3.2 Validation rules (auth + world) │ done
  3.3 Startup validation (auth + world, depends on 3.1, 3.2)│ anytime
                                                           ─┘

Phase 4 (Realm-Link Status) ───────────────────────────────┐
  4.1 Protocol extension (infrastructure) │
  4.2 Auth aggregation (infrastructure, depends on 4.1) │ Depends
  4.3 Auth TUI (auth exe, depends on 4.2) ─┘ on Phase 1
```

---

## Testing Strategy

Each phase follows TDD workflow (Red → Green → Refactor):

| Phase | Test Files |
|-------|-----------|
| 1.1 | `tests/unit/domain/MapSnapshotTests.cpp` |
| 1.2–1.3 | `tests/unit/application/MapServiceTests.cpp` |
| 1.4 | `tests/unit/application/MapRegistryTests.cpp` |
| 1.5 | `tests/unit/application/WorldServiceTests.cpp` (update existing) |
| 1.6 | `tests/unit/infrastructure/MapAuraTickerTests.cpp` |
| 2.1–2.2 | `tests/unit/infrastructure/AsyncNetworkServerTests.cpp` |
| 3.1 | `tests/unit/shared/ConfigValidatorTests.cpp` |
| 4.1 | `tests/unit/shared/RealmLinkProtocolTests.cpp` |

---

## Risk Assessment

| Risk | Mitigation |
|------|-----------|
| `WorldService` refactor breaks existing code | Small, incremental changes. Each method delegation is trivial. |
| Session tracking adds overhead | `weak_ptr` + `unordered_map` is O(1). Negligible. |
| Graceful shutdown hangs | Timeout is enforced. Falls back to abrupt stop. |
| Config validation breaks existing setups | Start with warnings-only, escalate to errors later. |
| FTXUI panel clutter | Make it collapsible or only show non-empty maps. |

---

## Recommended Starting Point

**Phase 1** — highest impact, most self-contained. `MapSnapshot` and `MapService` have zero infrastructure dependencies and can be built/tested in isolation before touching `WorldService`.
