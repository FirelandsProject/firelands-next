-- Wipe creature-related rows in firelands_world and unmark creature migrations so the
-- next auth or world server startup re-runs those SQL files (DatabaseMigrator).
--
-- Requires a MySQL user that can modify both `firelands_world` and `firelands_auth`.
-- Docker example (repo root):
--   docker compose exec -T db mysql -uroot -proot < tools/sql/reset_world_creatures_and_migrations.sql
--
-- After this: restart `world` (or `auth`; either runs migrator on `sql/init` + optional `sql/migrations`).
-- Creature bulk import: `python3 tools/sql/import_ref_creature_data.py` → `sql/bundled/creature_ref_import.sql`;
-- NPC text import: `python3 tools/sql/import_ref_npc_text.py` → `sql/migrations/34_world_npc_text_data.sql`;
-- Gossip import: `python3 tools/sql/import_ref_gossip.py` → `sql/migrations/35_world_gossip_data.sql`;
-- apply that file or merge into `sql/bundled/firelands_world.sql`, then refresh DB (e.g. docker volume reset).

USE `firelands_world`;

SET FOREIGN_KEY_CHECKS = 0;

DELETE FROM `creature_addon`;
DELETE FROM `creature`;
DELETE FROM `creature_template_addon`;
DELETE FROM `creature_template`;

SET FOREIGN_KEY_CHECKS = 1;

DELETE FROM `firelands_auth`.`schema_migrations`
WHERE `migration` IN (
  '23_world_creature_template_search.sql',
  '24_world_creature_tables.sql',
  '25_world_creature_classlevelstats.sql',
  '27_world_creature_extra_seed_spawns.sql',
  '28_world_creature_ref_import.sql',
  '29_world_creature_template_modelids.sql',
  '30_world_creature_ref_import.sql'
);
