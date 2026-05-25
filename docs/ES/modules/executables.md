# Ejecutables: `auth` y `world`

## `auth` (`src/auth/main.cpp`)

Servidor de **autenticación**:

1. Logger + **`authserver.yaml`** (se soporta override con `FIRELANDS_AUTH_CONFIG`).
2. Ejecuta **`DatabaseMigrator::MigrateAuthServerStartup`** — solo `firelands_auth` (init si la BD es nueva, luego migraciones pendientes).
3. Conecta a MariaDB y crea **`MySqlAccountRepository`** y **`MySqlRealmRepository`**.
4. Inicializa **`AuthService`** y **`RealmListService`** (opcionalmente con **`RealmLiveRegistry`** si `RealmLink.Token` y el puerto están configurados).
5. Inicia **`AsyncNetworkServer`** para clientes auth clásicos (`AuthSession`).
6. Opcional: listener **realm-link** (`RealmLinkSession`) para que `world` empuje población/carga.
7. Inicia **`RestAuthServer`** en `Network.RestPort` con **`WebSessionService`** + repo en memoria.

Bucle: se hace polling con `authServer.Update()` (y el server de realm-link si aplica).

## `world` (`src/world/main.cpp`)

Servidor de **mundo**:

1. Carga **`worldserver.yaml`** (`FIRELANDS_WORLD_CONFIG`) y falla si no existe.
2. Reconfigura logging (archivo, rotación, niveles).
3. Opcional: **`RunRealmLinkOutbound`** en un thread si `RealmLink.Enabled` es true; conecta a auth y envía estado usando `RealmLinkProtocol`.
4. Inicializa **`LuaGameScriptHost`** y lo registra en **`WorldService`**; dispara `world_startup`.
5. Configura **`MapCollisionQueriesStub`** desde `Collision.DataRoot`.
6. Migra SQL y conecta **tres** BDs: **auth**, **characters**, **world**.
7. Cablea servicios (auth/personajes, **`CommandService`** + **`OnlineCharacterSessionRegistry`**, playercreateinfo) y crea sesiones `WorldSession` en **`AsyncNetworkServer`**.

Bucle: `worldServer.Update()`.

Los comandos de staff con **`.` en el chat** y la **consola interactiva** (`WorldInteractiveConsole`) están descritos en [gm-administration.md](gm-administration.md).

## Vista operativa

- El cliente suele ir primero a **auth** y luego a **world**.
- **Realm-link** sincroniza métricas del world hacia auth cuando está habilitado.

