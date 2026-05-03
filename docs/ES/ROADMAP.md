# Firelands Next — Roadmap & Tracking (single source of truth)

Este documento es el **único lugar** para hacer seguimiento del progreso: roadmap general, estabilidad del cliente, y paridad vs referencia (incluyendo Lua).

**Referencia:** clone local de la implementación de referencia (mismo árbol clonado junto al repo; build 15595).

---

## Estado rápido (qué perseguimos)

- **Objetivo 0 (prioridad):** cliente **estable** (login → mundo → 5+ min idle) sin crashes y con UI predecible.
- **Objetivo 1:** paridad incremental por subsistemas (ver matriz).
- **Objetivo 2:** scripting en **Lua** para gameplay (sin Smart Scripts SQL).

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
- [ ] Quests/loot/menus (SMSG gossip/quest, aún pendiente)
- [ ] Instancias + fases (Lua)

---

## Client stability (track prioritario)

### Definition of done (short term)

- [x] Entrar al mundo sin crash (fix 2026-04-29)
- [ ] Permanecer conectado **≥ 5 min** idle
- [ ] Time sync “sano”: `SMSG_TIME_SYNC_REQ` con cadencia razonable (más allá del chain actual)
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

### Bitácora (stability)

| Fecha | Nota |
|------:|------|
| 2026-04-28 | TimeSync chain + unknown opcodes a debug |
| 2026-04-29 | Fix crash #132 (talents/specs + nativeDisplayId + bytes2) |
| 2026-04-29 | Mapeo/ignore de “client probes” post-login + primeros ACKs mínimos (mail/calendar/zoneupdate) |
| 2026-04-30 | CreateObject cruzado al login (`Map::ForEachPlayer`); checklist battlefield = no-op documentado |
| 2026-04-30 | CMSG_CAST_SPELL mínimo: wire 4.3.4 (`SpellCastWire`) + broadcast START/GO + GCD 1.5s |

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
| quests-instances | Quests/loot/gossip + instancias con Lua | Parcial (gossip CMSG→Lua; SMSG pendiente) |

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
| Quests / gossip | `QuestHandler`, `NPCHandler` | `CMSG_GOSSIP_*` → Lua `gossip_*` events | Started | `SMSG_GOSSIP_MESSAGE` + menu state |
| Loot | `LootMgr`, `Loot` | — | Not started | Basic take-item flow |
| Collision / path | `VMap`, `MMap`, `MapInstanced` | `IMapCollisionQueries` + stub | Started | Wire `Collision.DataRoot` to real queries |
| Instances / phases | `InstanceMap`, `InstanceScript` | — | Not started | Instance id on `Map` + reset hooks |
| Data stores (DBC) | `DB2Store`, SQL | — | Not started | Load critical templates for spells/units |
| Battlegrounds / arena | `Battleground*` | — | Not started | Out of scope until open world stable |
| Anticheat | `Anticheat` | — | Not started | Movement validation baseline |

**Priority order (short term):** world opcodes + visibility → creatures on map → combat stub → quests/gossip SMSG → collision data → instances.

**VMap / extractor pipeline (4.3.4):** see [`docs/VMAP_EXTRACTION_PLAN.md`](VMAP_EXTRACTION_PLAN.md) (mapextractor → vmap4extractor → vmap4assembler; mmaps later).

---

## Próximos pasos (orden sugerido)

1. ~~Completar **ACKs mínimos**~~ (incl. battlefield = no-op intencional sin cola).
2. ~~Dos clientes en el mismo mapa~~: CreateObject al login + movement/chat nearby.
3. ~~Empezar **spells mínimos**~~ (GCD + `SMSG_SPELL_START`/`GO` + fallos básicos); siguiente: **auras / efectos** o datos mínimos de hechizo (DBC/SQL).

