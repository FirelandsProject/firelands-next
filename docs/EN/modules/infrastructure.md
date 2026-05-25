# Module: `FirelandsInfrastructure` (`src/infrastructure`)

## Role

**Infrastructure** wires the emulator to the outside world: **MariaDB**, **Boost.Asio** TCP servers, optional **REST**, **realm-link** TCP between auth and world, **Lua** scripting, and **collision** stubs. It implements domain/application repository interfaces and session handlers.

## Persistence (`infrastructure/persistence/`)

- **`DatabaseMigrator`** — on startup, **auth** applies init + migrations for `firelands_auth` only; **world** applies init + migrations for `firelands_characters` and `firelands_world` only. Fresh DBs get `sql/init/` first; existing DBs get pending `sql/migrations/` only. `sql/bundled/` is for Docker first boot, not runtime. Tracks applied files in `firelands_auth.schema_migrations`.
- **`DatabaseService`** — connection/helper patterns as used by tooling or servers.
- **`MySqlAccountRepository`** — `IAccountRepository` against the auth DB (accounts, session keys, account_data).
- **`MySqlRealmRepository`** — `IRealmRepository` for `realmlist` (and related) rows.
- **`MySqlCharacterRepository`** — `ICharacterRepository` for the characters database.
- **`MySqlPlayerCreateInfoRepository`** — world DB static templates for new characters.
- **`MemoryWebSessionRepository`** — ephemeral REST sessions for Dev/API flows.

## Network (`infrastructure/network/`)

All socket I/O in this tree uses **C++20 coroutines** (`co_await`, `boost::asio::use_awaitable`). Shared helpers live in **`AsioAwaitables.h`**.

- **`AsyncNetworkServer`** — coroutine `AcceptLoop`; `Update()` polls `io_context`.
- **`AuthSession`** — `ReadLoop` + queued `WriteLoop`.
- **`WorldSession`** — `ReadLoop`, `WriteLoop`, `TimeSyncLoop`, deferred spell completion via `co_await` on `_pendingSpellCastTimer`.
- **`RestAuthServer`** — `AcceptLoop`, per-client `ServeClient` (`async_read` / `async_write`).
- **Realm link** — `RealmLinkSession` (`ReadLoop`, `co_await SendAck`); `RealmLinkOutbound` coroutine session on world (handshake, ping loop).

## Scripting & world adapters

- **`LuaGameScriptHost`** — loads Lua under `Scripting.ScriptsDirectory`; fires events (`world_startup`, spawn hooks, etc.) via `IGameScriptHost`.
- **`MapCollisionQueriesStub`** — placeholder `IMapCollisionQueries` implementation gated by config paths until full vmap integration.

## CMake

`FirelandsInfrastructure` links **FirelandsApplication**, MariaDB C++, Boost thread, nlohmann_json, **Lua**, zlib; includes connector headers. `LuaGameScriptHost.cpp` skips PCH for toolchain compatibility.
