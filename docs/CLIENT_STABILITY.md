# Client stability roadmap (4.3.4)

Tracking file for **stable client** priority: login, stay in world, and predictable handling of common packets. Update checkboxes as work lands.

**Related:** [implementation_plan.md](../implementation_plan.md), [parity_matrix.md](parity_matrix.md), [PARITY_AND_LUA_ROADMAP.md](PARITY_AND_LUA_ROADMAP.md).

---

## Definition of done (short term)

- [x] Client reaches world after `CMSG_PLAYER_LOGIN` without client crash (no WoWError #132 on null spell book / known traps). *Fixed 2026-04-29.*
- [ ] Client remains connected **≥ 5 minutes** idle in world (no EOF from server-side close).
- [ ] **Time sync loop:** server issues `SMSG_TIME_SYNC_REQ` on a sensible cadence; client `CMSG_TIME_SYNC_RESP` handled without desync warnings. *(Chain after each RESP implemented; optional timer TBD.)*
- [ ] **High-frequency CMSG** after login either implemented (ACK/minimal) or safely ignored (no assert, no tight spin): account data, voice/violence, loading screen, etc.
- [x] **Unknown opcodes:** logged at **debug** level (`WorldSession::ProcessPacket` default case).
- [ ] **SMSG order and payloads** for login burst spot-checked vs `firelands-cata-ref` `Player::SendInitialPacketsBeforeAddToMap` / `AfterAddToMap`.

---

## Phase A — Login burst (already mostly done)

- [x] Order of SMSG around `CMSG_PLAYER_LOGIN` aligned with ref comments in `WorldSession::HandlePlayerLogin`.
- [x] Known spells non-empty per class (`BuildDefaultKnownSpells`).
- [ ] Re-verify `SMSG_UPDATE_OBJECT` create payload vs ref for ** TYPEID_PLAYER** (field count / flags) after any UpdateFields change.
- [ ] Addon / cache version packets accepted by client (`SMSG_ADDON_INFO` if required by your build).

---

## Phase B — Post-login chatter

- [x] `CMSG_QUERY_TIME`, `CMSG_PLAYED_TIME`, `CMSG_NAME_QUERY`, `CMSG_UPDATE_ACCOUNT_DATA` ACK paths.
- [x] Audit client capture for **first 30s** in world: list unknown opcodes → map them to ref names and add safe no-op handlers.
- [ ] Replace no-op handlers with **minimal ACK responses** where the client expects a reply (mail/calendar/guild/battlefield/LFG).
- [ ] `CMSG_READY_FOR_ACCOUNT_DATA_TIMES` / account data mask vs client expectations.

### Post-login “probes” — minimal ACK checklist

Goal: keep the client UI consistent (no spinning panels / stuck states) without implementing full systems yet.

- [x] **Mail time**: `MSG_QUERY_NEXT_MAIL_TIME` → `MSG_QUERY_NEXT_MAIL_TIME` response (0 / no mail).
- [x] **Calendar pending**: `CMSG_CALENDAR_GET_NUM_PENDING` → `SMSG_CALENDAR_SEND_NUM_PENDING` (0 pending).
- [ ] **Guild bank withdraw query**: `CMSG_GUILD_BANK_REMAINING_WITHDRAW_MONEY_QUERY` → response (0 / not in guild).
- [ ] **Battlefield status/state**: `CMSG_BATTLEFIELD_STATUS`, `CMSG_QUERY_BATTLEFIELD_STATE` → “not in queue / not in BG”.
- [ ] **LFG**: `CMSG_LFG_GET_STATUS`, `CMSG_LFG_LOCK_INFO_REQUEST` → minimal “not in group / locked info empty”.
- [x] **Zone update**: `CMSG_ZONEUPDATE` stores latest zoneId on session (future: push into Player state + Lua hook).

---

## Phase C — Time sync and keepalive

- [x] Send follow-up `SMSG_TIME_SYNC_REQ` after each `CMSG_TIME_SYNC_RESP` (chain keeps client clock authority fresh).
- [ ] Optional: periodic time sync on timer if ref sends on interval independent of client (future: session-bound `steady_timer` on socket executor).
- [x] `CMSG_PING` → `SMSG_PONG` (already present).

---

## Phase D — Multi-session and visibility

- [x] Movement relay to nearby cells (`BroadcastPacketToNearby`).
- [ ] Two clients same map: both see movement and SAY/YELL (manual QA checklist).
- [ ] Graceful disconnect on malformed packet (log + close, no server abort).

---

## Phase E — Diagnostics

- [x] Log last SMSG opcode/size on disconnect (already in session diagnostics).
- [ ] One-page **manual QA script** (login → move → chat → logout) linked from here or `docs/devtools.md`.

---

## Bitácora

| Date | Note |
|------|------|
| 2026-04-28 | Created `CLIENT_STABILITY.md`; chained `SMSG_TIME_SYNC_REQ` after `CMSG_TIME_SYNC_RESP`; unknown opcodes at `LOG_DEBUG`. |
| 2026-04-29 | **WoWError #132 crash at end of loading screen fixed** — three root causes: (1) `SMSG_TALENTS_INFO` enviaba `specsCount=0`; cliente 4.3.4 accede `specs[activeSpec]` OOB → ahora envía 1 spec vacío con 6 glyphs nulos. (2) `SMSG_UPDATE_OBJECT` faltaba `UNIT_FIELD_NATIVEDISPLAYID` (crash en transform auras). (3) Faltaba `UNIT_FIELD_BYTES_2` (PvP flags). Todos los 47 tests pasan. |
| 2026-04-29 | Mapeo y no-op handlers para opcodes “probe” vistos post-login: `CMSG_SET_ACTIVE_MOVER`, `CMSG_SET_ACTIONBAR_TOGGLES`, `CMSG_DB_QUERY_BULK`, `CMSG_CALENDAR_GET_NUM_PENDING`, `CMSG_LFG_*`, `CMSG_BATTLEFIELD_*`, `MSG_QUERY_NEXT_MAIL_TIME`, `CMSG_ZONEUPDATE`, etc. Siguiente paso: convertirlos a ACKs mínimos cuando aplique. |
