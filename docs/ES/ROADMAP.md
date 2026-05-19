# Firelands Next — Roadmap & Tracking (single source of truth)

Este documento es el **único lugar** para hacer seguimiento del progreso: roadmap general, estabilidad del cliente, y paridad vs referencia (incluyendo Lua).

**Referencia:** clone local de la implementación de referencia (mismo árbol clonado junto al repo; build 15595).

---

## Estado rápido (qué perseguimos)

- **Objetivo 0 (prioridad):** cliente **estable** (login → mundo → 5+ min idle) sin crashes y con UI predecible.
- **Objetivo 1:** paridad incremental por subsistemas (ver matriz).
- **Objetivo 2:** scripting en **Lua** para gameplay (sin Smart Scripts SQL).

---

## Estado del workspace (documentado 2026-05-18)

- **Último commit (`d5b48b1`):** menús de gossip NPC desde world DB — ver [gossip-npc-text.md](../EN/modules/gossip-npc-text.md#shipped-d5b48b1--npc-gossip-menus).
- **En curso (sin commitear):** tabla y datos `npc_text` + `CMSG_NPC_TEXT_QUERY` → `SMSG_NPC_TEXT_UPDATE` (copy del diálogo de gossip); migraciones `33`/`34`, repo MySQL, tests. Detalle: [gossip-npc-text.md](../EN/modules/gossip-npc-text.md#in-progress--npc_text-dialog-copy).
- **Refactor de red (sigue abierto):** split `WorldSession` en `worldsession/*.cpp` + headers en `src/shared/network/packets/` (login, handlers, SMSG auxiliares).
- **Regenerar bundles SQL** tras cerrar `npc_text` / datos gossip: `python3 tools/merge_migrations.py` o `cmake --build build --target merge-migrations`.

### Snapshot anterior (2026-05-03)

- Modularización `WorldSession` + capa `shared/network/packets/` (packed player GUID, probes post-login).
- Motivo: matriz **Opcodes / packets** tipados sin cambiar comportamiento observable.

---

## Roadmap por fases (alto nivel)

### Phase 1 — Foundations & Auth (COMPLETED)
- [x] Project skeleton (CMake / C++17)
- [x] Logging (`spdlog`)
- [x] DB base (MariaDB/MySQL connectors)
- [x] SRP6 auth + auth success

### Phase 2 — Realm System (COMPLETED)
- [x] `realmlist` table
- [x] `CMD_REALM_LIST` + `SMSG_REALM_LIST`

### Phase 3 — World server skeleton (COMPLETED)
- [x] `worldserver` app + YAML config
- [x] `CMSG_AUTH_SESSION` + session validation

### Phase 4 — Character selection & management (COMPLETED)
- [x] Characters DB schema
- [x] `CMSG_CHAR_ENUM` / create / delete

### Phase 5 — Entering the world (IN PROGRESS)
- [x] `CMSG_PLAYER_LOGIN` core flow
- [x] Login burst SMSG order (alineado a ref a nivel macro)
- [x] Spawn inicial: `SMSG_UPDATE_OBJECT` (CreateObject player)
- [x] Movement relay + chat base
- [x] Fix crash #132 en fin de loading (talents/specs + nativeDisplayId + bytes2)
- [x] Post-login probes: **no-op safety net** (mapeo y ignore de opcodes vistos en logs)
- [x] Post-login probes: **ACK/minimal responses** donde el cliente espera reply (ver checklist abajo)
- [x] Dos clientes mismo mapa: **CreateObject cruzado** al login (movimiento/chat nearby ya existían)

### Phase 6 — Gameplay mechanics (IN PROGRESS)
- [x] Matriz de paridad (inline en este documento)
- [x] Lua scripting host (MVP) + tests
- [x] WorldService wiring + hooks Lua (login/gossip/movement/chat/spawn)
- [x] Spells — cast mínimo (GCD + `SMSG_SPELL_START`/`GO` + `SMSG_SPELL_FAILURE` vía lista conocida); auras/DBC pendientes
- [x] Gossip DB — `SMSG_GOSSIP_MESSAGE` / `SMSG_GOSSIP_COMPLETE`, menús desde `gossip_menu*` (commit `d5b48b1`)
- [ ] Gossip — copy de diálogo vía `npc_text` + `SMSG_NPC_TEXT_UPDATE` (en curso, ver módulo gossip)
- [ ] Quests/loot en menú gossip (`SMSG_GOSSIP_MESSAGE` quest lines) y flujo quest
- [ ] Instancias + fases (Lua)

---

## Client stability (track prioritario)

### Definition of done (short term)

- [x] Entrar al mundo sin crash (fix 2026-04-29)
- [x] Permanecer conectado **≥ 5 min** idle (validación manual; ver abajo)
- [x] Time sync “sano”: `SMSG_TIME_SYNC_REQ` por **timer periódico** (no encadenado en cada `CMSG_TIME_SYNC_RESP`); cadencia configurable `Network.TimeSyncPeriodMs` (2 s–1 h, defecto en yaml **5 min** / 300000 ms)
- [ ] Post-login chatter: implementado o ignorado de forma segura (sin loops, sin asserts)
- [ ] Validar payloads/order del login burst vs ref (spot-check)

### Post-login “probes” — minimal ACK checklist

Goal: mantener UI consistente sin implementar sistemas completos todavía.

- [x] **Mail time**: `MSG_QUERY_NEXT_MAIL_TIME` → response (0 / no mail)
- [x] **Calendar pending**: `CMSG_CALENDAR_GET_NUM_PENDING` → `SMSG_CALENDAR_SEND_NUM_PENDING` (0)
- [x] **Zone update**: `CMSG_ZONEUPDATE` guarda `zoneId` en sesión (futuro: estado Player + hook Lua)
- [x] **Guild bank withdraw query**: `CMSG_GUILD_BANK_REMAINING_WITHDRAW_MONEY_QUERY` → `SMSG_GUILD_BANK_MONEY_WITHDRAWN` (0)
- [x] **Battlefield status/state**: `CMSG_BATTLEFIELD_STATUS`, `CMSG_QUERY_BATTLEFIELD_STATE` — **sin SMSG** si no hay cola/BG (misma ref.); se mantienen como no-op seguro
- [x] **LFG**: `CMSG_LFG_GET_STATUS` → `SMSG_LFG_UPDATE_STATUS_NONE`; `CMSG_LFG_LOCK_INFO_REQUEST` → `SMSG_LFG_PLAYER_INFO`/`SMSG_LFG_PARTY_INFO` vacíos
- [x] **Cemetery list**: `CMSG_REQUEST_CEMETERY_LIST` → `SMSG_REQUEST_CEMETERY_LIST_RESPONSE` (lista vacía)

### Validación idle (manual, ≥ 5 min)

1. Entrar al mundo y permanecer inmóvil (sin chat ni comandos) al menos **5 minutos**.
2. Confirmar que el cliente no se cae y que el servidor no registra cierre de socket inesperado (`Session disconnect` / errores de lectura).
3. Opcional: `Log.Level: trace` y revisar `CMSG_TIME_SYNC_RESP` periódicos; ajustar `Network.TimeSyncPeriodMs` en `worldserver.yaml` si hace falta (p. ej. 5000 para cadencia tipo referencia).

### Bitácora (stability)

| Fecha | Nota |
|------:|------|
| 2026-04-28 | TimeSync chain + unknown opcodes a debug |
| 2026-04-29 | Fix crash #132 (talents/specs + nativeDisplayId + bytes2) |
| 2026-04-29 | Mapeo/ignore de “client probes” post-login + primeros ACKs mínimos (mail/calendar/zoneupdate) |
| 2026-04-30 | CreateObject cruzado al login (`Map::ForEachPlayer`); checklist battlefield = no-op documentado |
| 2026-04-30 | CMSG_CAST_SPELL mínimo: wire 4.3.4 (`SpellCastWire`) + broadcast START/GO + GCD 1.5s |
| 2026-05-03 | Doc: snapshot refactor capa paquetes (`shared/network/packets/*`) + split `WorldSession`; foco siguiente alineado con estabilidad idle / auras |
| 2026-05-03 | Estabilidad: `Network.TimeSyncPeriodMs` + cancel defensivo antes de `SchedulePeriodicTimeSync`; trace en `CMSG_TIME_SYNC_RESP`; guía de validación idle en roadmap |
| 2026-05-05 | Estabilidad: validación idle ≥ 5 min completada (cliente estable) |
| 2026-05-18 | Gossip: commit `d5b48b1` — menús desde world DB (`gossip_menu*`), paquetes 4.3.4, Lua + fallback, `.npc search` mejorado |
| 2026-05-18 | Gossip (WIP): `npc_text` + `CMSG_NPC_TEXT_QUERY` / `SMSG_NPC_TEXT_UPDATE`; migraciones 33–34 + import ref |

---

## Paridad vs referencia + Lua

### Hitos “paridad + Lua”

| ID | Hito | Estado |
|----|------|--------|
| parity-matrix | Matriz de paridad (subsistema × ref × next × criterio) | Hecho (inline en este documento) |
| lua-foundation | Lua 5.4 + `IGameScriptHost` + `LuaGameScriptHost` + tests | Hecho (MVP) |
| world-core-gap | Cerrar gaps fase 5–6 (opcodes/mundo vacío/broadcast) | En curso |
| entities-combat | Creature/GO + combate/hechizos mínimos + hooks Lua | Parcial (dominio + spawn hooks; cast START/GO + GCD) |
| maps-collision | mmap/vmap + colisión alineada a ref | Stub listo; integración real pendiente |
| quests-instances | Quests/loot/gossip + instancias con Lua | Parcial (gossip SMSG menú DB; `npc_text` en curso; quests en menú pendiente) |

### Matriz de paridad (subsistema × ref × next × criterio)

Living section: actualizar **Status** y **Next criterion** al cerrar hitos.

| Subsystem | Reference (primary paths) | firelands-next | Status | Next criterion |
|-----------|---------------------------|----------------|--------|----------------|
| Auth / SRP | `authserver`, `SRP6` | `AuthSession`, `SRPService` | Done | Client login stable |
| Realm list | `RealmList`, handlers | `RealmListService`, `AuthSession` | Done | Match packet fields vs ref |
| World socket / crypto | `WorldSocket`, `WorldCrypt` | `WorldSession`, `WorldCrypt` | Partial | Full header edge cases vs ref |
| Opcodes / packets | `Opcodes.cpp`, `Packets/` | `WorldOpcodes.h`, `WorldPacket`, handlers | Partial | Opcode coverage matrix per login + world |
| Character DB / enum | `CharacterDatabase`, handlers | `MySqlCharacterRepository`, `CharacterService` | Done | Schema parity with ref SQL |
| Player login sequence | `CharacterHandler`, `Player::SendInitialPackets*` | `WorldSession::HandlePlayerLogin` | Partial | Byte-for-byte spot-check critical SMSG vs ref |
| Movement | `MovementHandler`, `Map` | `HandleMovement`, `Map::UpdateObjectPosition` | Partial | Validate opcode filter + anti-cheat hooks |
| Map / grid | `Map`, `Grid`, `Object` | `Map`, `WorldObject`, `Player` | Partial | Multi-map instance IDs (see Instances) |
| Visibility / broadcast | `Map::SendToPlayers`, grid visibility | `BroadcastPacket`, `BroadcastPacketToNearby` | Partial | SAY/YELL nearby (implemented); true visibility range later |
| Chat | `ChatHandler` | `HandleMessageChat` | Partial | Guild/party/whisper parity |
| Scripting / hooks | `ScriptMgr`, AI scripts | `IGameScriptHost`, `LuaGameScriptHost`, Lua `OnScriptEvent` | Partial | Expand C++→Lua surface + sandbox |
| Creatures / GOs | `Creature`, `GameObject`, spawns | `Creature`, `GameObject` domain types | Started | Spawn pipeline + `SMSG_UPDATE_OBJECT` for units |
| Combat / spells | `Spell`, `Unit`, `Aura` | `SpellCastWire`, `WorldSession::HandleCastSpell` | Started | Aura aplicada + coste/recovery desde datos |
| Quests / gossip | `QuestHandler`, `NPCHandler` | `CMSG_GOSSIP_*` + `IGossipRepository`; Lua primero, fallback DB | Partial | `npc_text` + quest lines en `SMSG_GOSSIP_MESSAGE`; ver [gossip-npc-text.md](../EN/modules/gossip-npc-text.md) |
| Loot | `LootMgr`, `Loot` | — | Not started | Basic take-item flow |
| Collision / path | `VMap`, `MMap`, `MapInstanced` | `IMapCollisionQueries` + stub | Started | Wire `Collision.DataRoot` to real queries |
| Instances / phases | `InstanceMap`, `InstanceScript` | — | Not started | Instance id on `Map` + reset hooks |
| Data stores (DBC) | `DB2Store`, SQL | — | Not started | Load critical templates for spells/units |
| Battlegrounds / arena | `Battleground*` | — | Not started | Out of scope until open world stable |
| Anticheat | `Anticheat` | — | Not started | Movement validation baseline |

**Priority order (short term):** cerrar `npc_text` (commit + bundles) → quest lines en gossip → combat/auras → collision data → instances.

**Extractores / colisión (4.3.4):** plan maestro en [`docs/EN/VMAP_EXTRACTION_PLAN.md`](../EN/VMAP_EXTRACTION_PLAN.md); resumen ES en [`docs/ES/VMAP_EXTRACTION_PLAN.md`](VMAP_EXTRACTION_PLAN.md) (herramientas 1–4 + runtime + cierre).

---

## Próximos pasos (orden sugerido)

1. ~~Completar **ACKs mínimos**~~ (incl. battlefield = no-op intencional sin cola).
2. ~~Dos clientes en el mismo mapa~~: CreateObject al login + movement/chat nearby.
3. ~~Empezar **spells mínimos**~~ (GCD + `SMSG_SPELL_START`/`GO` + fallos básicos); siguiente: **auras / efectos** o datos mínimos de hechizo (DBC/SQL).
4. **Cerrar el refactor de red** (split `WorldSession` + headers en `shared/network/packets/`): revisar CMake/includes, compilar y commitear; seguir extrayendo lecturas/escrituras repetidas a tipos compartidos donde aporte claridad.
5. **Estabilidad cliente (Definition of done):** sesión idle **≥ 5 min**, cadencia sana de `SMSG_TIME_SYNC_REQ`, y validación puntual del burst de login vs referencia.
6. **Gossip / NPC text:** commitear trabajo `npc_text` en curso; regenerar `sql/bundled/firelands_world.sql`; validar in-game: abrir gossip en NPC con `gossip_menu_id` y texto de `TextID` cargado por el cliente.

