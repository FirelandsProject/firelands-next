# Executables: `auth` and `world`

## `auth` (`src/auth/main.cpp`)

The **authentication server** executable:

1. Initializes logging, loads **`authserver.yaml`** (`FIRELANDS_AUTH_CONFIG` override supported).
2. Runs **`DatabaseMigrator::MigrateAuthServerStartup`** — `sql/init/auth_schema.sql` on a fresh `firelands_auth`, then pending `sql/migrations/*_auth_*.sql` only (bundled SQL is Docker-only).
3. Opens a MariaDB connection and constructs **`MySqlAccountRepository`**, **`MySqlRealmRepository`**.
4. Builds **`AuthService`**, **`RealmListService`** (optionally with **`RealmLiveRegistry`** when `RealmLink.Token` + port are set).
5. Starts **`AsyncNetworkServer`** for classic auth clients (`AuthSession` factory).
6. Optionally starts **realm-link** listener (`RealmLinkSession`) for worlds to push live population/load.
7. Starts **`RestAuthServer`** on `Network.RestPort` for REST login/session flows with **`WebSessionService`** + in-memory repo.

Main loop: poll `authServer.Update()` (and realm-link server if enabled); REST runs on its own acceptance path inside `RestAuthServer`.

## `world` (`src/world/main.cpp`)

The **world server** executable:

1. Loads **`worldserver.yaml`** (`FIRELANDS_WORLD_CONFIG`); exits if missing (stricter than auth defaults).
2. Reconfigures logging (file rotation, levels).
3. Optionally starts **`RunRealmLinkOutbound`** on a background thread when `RealmLink.Enabled` is true—connects to auth’s realm-link port and sends heartbeat/state using shared **`RealmLinkProtocol`**.
4. Initializes **`LuaGameScriptHost`** and assigns it to **`WorldService`**; fires `world_startup`.
5. Sets **`MapCollisionQueriesStub`** from `Collision.DataRoot`.
6. Runs **`DatabaseMigrator::MigrateWorldServerStartup`** for **characters** and **world** (init on fresh DBs, then pending world/characters migrations; uses auth URI only for `schema_migrations` bookkeeping), then connects **three** logical databases: **auth**, **characters**, **world**.
7. Wires **`AuthService`**, **`CharacterService`**, **`CommandService`** + **`OnlineCharacterSessionRegistry`**, **`PlayerCreateInfoService`**, and **`WorldSession`** factory on **`AsyncNetworkServer`** (`Network.Port`, default world listener distinct from auth).

Main loop: `worldServer.Update()` polling.

Staff **`.` chat commands** and the **interactive console** (`WorldInteractiveConsole`) are documented in [gm-administration.md](gm-administration.md).

## Operational picture

- Clients typically hit **auth** first (SRP, realm list), then connect to **world** with session-derived crypto.
- **Realm-link** synchronizes live realm metrics from world to auth when configured.

