# Roadmap (EN)

The roadmap currently lives in Spanish under [docs/ES/ROADMAP.md](../ES/ROADMAP.md) and is the **single source of truth**.

If you prefer, we can later translate the full roadmap into English and keep both versions in sync.

## Workspace snapshot (doc update 2026-05-18)

Full tracking lives in [docs/ES/ROADMAP.md](../ES/ROADMAP.md) (single source of truth).

- **Last commit (`d5b48b1`):** NPC gossip menus from world DB — `SMSG_GOSSIP_MESSAGE` / `SMSG_GOSSIP_COMPLETE`, Lua-first then `IGossipRepository` fallback, migrations `31`–`32` + gossip data import. See [modules/gossip-npc-text.md](modules/gossip-npc-text.md#shipped-d5b48b1--npc-gossip-menus).
- **In progress (uncommitted):** `npc_text` table + `CMSG_NPC_TEXT_QUERY` → `SMSG_NPC_TEXT_UPDATE` for dialog copy; migrations `33`–`34`, `MySqlNpcTextRepository`, unit tests. See [gossip-npc-text.md](modules/gossip-npc-text.md#in-progress--npc_text-dialog-copy).
- **Still open:** `WorldSession` split + `shared/network/packets/` refactor; regenerate `sql/bundled/firelands_world.sql` after `npc_text` lands.
- **2026-05-03 (stability):** idle ≥5 min validated; `Network.TimeSyncPeriodMs` in `worldserver.yaml` (see Spanish roadmap).

