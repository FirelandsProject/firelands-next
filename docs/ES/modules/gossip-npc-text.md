# Gossip NPC y `npc_text`

Gossip Cataclysm **4.3.4**: menús desde world DB, texto de diálogo, hooks Lua y paquetes alineados a referencia Trinity/TCPP.

**Seguimiento:** [ROADMAP.md](../ROADMAP.md) (snapshot del workspace, matriz de paridad, bitácora).

La referencia técnica completa (inglés, misma estructura) está en [docs/EN/modules/gossip-npc-text.md](../../EN/modules/gossip-npc-text.md).

---

## Entregado (`d5b48b1`) — menús de gossip

**Commit:** `d5b48b1` — `feat(world): implement NPC gossip menus`

- **Flujo:** Lua (`gossip_hello`) → fallback `gossip_menu*` por `creature_template.gossip_menu_id` → `SMSG_GOSSIP_MESSAGE` o `SMSG_GOSSIP_COMPLETE`.
- **Selección:** `CMSG_GOSSIP_SELECT_OPTION` → Lua + menús encadenados (`ActionMenuId`).
- **SQL:** migraciones `31`, `32`; datos gossip `35` vía `python3 tools/sql/import_ref_gossip.py`.
- **Tests:** `GossipLogicTests`, `GossipMenuTests`, `GossipPacketTests`, `WowGuidTests`.
- **GM:** `.npc search` con salida coloreada en chat de sistema.

Detalle de archivos, opcodes y tablas: sección *Shipped* en el doc EN enlazado arriba.

---

## En curso — `npc_text`

**Estado:** cambios locales **sin commitear** (2026-05-18).

- Cliente pide copy con `CMSG_NPC_TEXT_QUERY`; servidor responde `SMSG_NPC_TEXT_UPDATE` (8 slots).
- Repo: `INpcTextRepository` / `MySqlNpcTextRepository`; modelo `NpcText`.
- SQL: migraciones `33` (DDL), `34` (datos con `import_ref_npc_text.py`).
- Pendiente: commit, `merge-migrations`, prueba in-game con `TextID` real.

Criterios de cierre y diagrama de flujo: doc EN § *In progress*.

---

## Lua

Ver [LUA_SCRIPTING.md](../LUA_SCRIPTING.md) (`on_gossip_hello`, `on_gossip_select`, `GossipSendMenu(npc_text, …)`).
