# GM ticket system (design)

This document specifies **persistence**, **assignment**, **application layer**, and **networking (4.3.4 / 15595)** for WoW-style help tickets. It complements [gm-administration.md](gm-administration.md) (`.` commands and world console).

## Goals

- Players **open tickets** from the client help UI; the server **persists** them and answers with the expected `SMSG_GM_TICKET_*` sequence.
- Staff (accounts with the right permissions) **claim** queue items, **reply**, and **close**; players see replies and can **resolve** (`CMSG_GM_TICKET_RESPONSE_RESOLVE`).
- Queue and history live in **`firelands_characters`** (same DB as characters).

## Persistence

### `gm_ticket` table

Defined in `sql/18_gm_ticket.sql` and mirrored in `sql/characters_schema.sql` for fresh installs.

| Column | Purpose |
|--------|---------|
| `id` | Stable ticket id. |
| `account_id` | Player account (`account.id` in auth DB). |
| `character_guid` | Character row; FK to `characters.guid`. |
| `status` | See `GmTicketStatus` in `domain/models/GmTicket.h`. |
| `category` | Client category byte (map from real wire payload). |
| `message` | Player text (length / sanitization like chat). |
| `gm_response` | Last staff reply for `SMSG_GMRESPONSE_*` when applicable. |
| `map_id`, `pos_*` | Snapshot at creation (GM teleport / context). |
| `assigned_account_id` | Assigned GM `account.id`, or `NULL` in the open queue. |
| `created_at`, `updated_at`, `assigned_at`, `closed_at` | Audit and FIFO ordering. |

### `GmTicketStatus`

- **Open** — in queue, unassigned (unless you choose a different policy).
- **Assigned** — `assigned_account_id` set.
- **GmAnswered** — staff reply stored; client may show survey / resolve flow.
- **ClosedResolved** — player resolved after reply.
- **ClosedAbandoned** — player deleted ticket or abandoned.
- **ClosedStaff** — staff closed without player resolve (internal tool / command).

Tighten semantics per handler once client expectations for each `SMSG_*` are verified.

### Repository

- Interface: `domain/repositories/IGmTicketRepository.h`.
- Planned implementation: `MySqlGmTicketRepository` (same JDBC stack as `MySqlCharacterRepository`).
- **`TryAssign`**: `UPDATE ... SET assigned_account_id = ?, status = 1, assigned_at = NOW() WHERE id = ? AND assigned_account_id IS NULL AND status = 0` and require `affected_rows == 1` to avoid double-claim races.

## Assignment and business rules

1. **One active ticket per character** (recommended): before `Insert`, check `FindOpenByCharacterGuid` and return a wire error if one exists.
2. **Queue**: `ListUnassignedOpen(N)` ordered by `created_at` ASC; staff pulls via console, `.` commands, or a future addon UI.
3. **Claim**: `GmTicketService::Assign(ticketId, staffAccountId)` validates **AccessLevel** / **Permission**, calls `TryAssign`, optionally notifies the player (`SMSG_GM_TICKET_STATUS_UPDATE` or chat).
4. **Reply**: update `gm_response`, set **GmAnswered**, send **`SMSG_GMRESPONSE_RECEIVED`** with the correct 4.3.4 layout (see below).
5. **GM disconnect**: choose policy (keep assignment vs revert to Open); document in code when a `WorldSession` hook exists.

## Network (15595 opcodes)

Constants live in `shared/network/WorldOpcodes.h` (source: WowPacketParser `V4_3_4_15595/Opcodes.cs`).

### Client → server

| Opcode | Value | Role |
|--------|-------|------|
| `CMSG_GM_TICKET_CREATE` | 0x0137 | Create (map, pos, text, flags, optional compressed log). |
| `CMSG_GM_TICKET_UPDATE_TEXT` | 0x0636 | Update message. |
| `CMSG_GM_TICKET_DELETE_TICKET` | 0x6B14 | Delete / abandon. |
| `CMSG_GM_TICKET_GET_TICKET` | 0x0326 | Query current ticket (was a no-op in dispatcher). |
| `CMSG_GM_TICKET_GET_SYSTEM_STATUS` | 0x4205 | Queue enabled / disabled. |
| `CMSG_GM_TICKET_RESPONSE_RESOLVE` | 0x6506 | Player marks resolved (often empty body). |
| `CMSG_GM_SURVEY_SUBMIT` | 0x2724 | Post-resolution survey (optional phase 2). |

### Server → client

| Opcode | Value | Role |
|--------|-------|------|
| `SMSG_GM_TICKET_CREATE` | 0x2107 | Create result code. |
| `SMSG_GM_TICKET_UPDATE_TEXT` | 0x6535 | Update result. |
| `SMSG_GM_TICKET_DELETE_TICKET` | 0x6D17 | Delete ack. |
| `SMSG_GM_TICKET_GET_TICKET` | 0x2C15 | Ticket payload or “none”. |
| `SMSG_GM_TICKET_GET_SYSTEM_STATUS` | 0x0D35 | Enable / disable UI. |
| `SMSG_GM_TICKET_STATUS_UPDATE` | 0x2C25 | Queue / status notification. |
| `SMSG_GMRESPONSE_RECEIVED` | 0x2E34 | GM reply to player. |
| `SMSG_GMRESPONSE_STATUS_UPDATE` | 0x0A04 | After resolve (e.g. survey gate). |
| `SMSG_GMRESPONSE_DB_ERROR` | 0x0006 | Backend error (use sparingly). |

### Packet layout

Do not copy 3.3.5-era handlers blindly; Cataclysm support changed. Recommended workflow:

1. Use WowPacketParser structs for **15595** or capture sniff from a 4.3.4 reference realm.
2. Implement read/write in `WorldSession` or a small `GmTicketPackets.{h,cpp}` helper with unit tests on **fixed hex blobs** under `tests/data/`.
3. Wire cases in `WorldSessionDispatcher.cpp` instead of the current no-op `break`.

## Application layer (next coding steps)

1. **`GmTicketService`** (application): orchestrates repository + validation + “one active ticket per character”; no sockets.
2. **`WorldSession`**: parses CMSG, calls service, sends SMSG; checks `_accountId` / `_playerGuid` for players and `GetAccountAccessLevel()` for staff paths.
3. **Permissions**: add e.g. `ManageGmTickets` in `Permissions.h`, default on `GameMaster`, plus `.ticket queue` / `.ticket take` / `.ticket close` calling the same service (console + in-game).
4. **Wiring**: in `world/main.cpp`, construct `MySqlGmTicketRepository(charConn)` and pass `shared_ptr<GmTicketService>` into the `WorldSession` factory (constructor growth or a dependencies struct).

## Related docs

- [gm-administration.md](gm-administration.md) — permissions and existing commands.
- [application.md](application.md) — hexagonal layout and services.

## Implementation checklist

- [x] `MySqlGmTicketRepository` (`sql/18_gm_ticket.sql`, `need_more_help` via `Ensure` when upgrading an older table).
- [x] `GmTicketService` (rules + `TryAssign` in SQL).
- [x] Core CMSG / SMSG path (`WorldSessionGmTicketHandlers.cpp`, `GmTicketPackets.cpp`) following WowPacketParser 4.3.x layout.
- [x] In-game `.ticket` commands (`CommandService`, `ManageGmTickets` permission).
- [x] `.ticket ui` — gossip desk (`WorldSessionGmTicketGossip.cpp`, synthetic menu/text ids in `GmTicketGossipUi.h`).
- [ ] reference hyperlink sanitization (optional hardening).
