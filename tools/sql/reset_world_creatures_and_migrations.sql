-- Wipe creature-related rows in firelands_world and unmark creature migrations so the
-- next auth or world server startup re-runs those SQL files (DatabaseMigrator).
--
-- Requires a MySQL user that can modify both `firelands_world` and `firelands_auth`.
-- Docker example (repo root):
--   docker compose exec -T db mysql -uroot -proot < tools/sql/reset_world_creatures_and_migrations.sql
--
-- After this: restart `world` (or `auth`; either runs migrator on `sql/`).
-- Creature bulk import: `python3 tools/sql/import_ref_creature_data.py`
--   → `sql/bundled/firelands_world_creature_ref_import.sql`; then refresh DB (e.g. docker volume reset).

USE `firelands_world`;

SET FOREIGN_KEY_CHECKS = 0;

DELETE FROM `creature_addon`;
DELETE FROM `creature`;
DELETE FROM `creature_template_addon`;
DELETE FROM `creature_template`;

SET FOREIGN_KEY_CHECKS = 1;

DELETE FROM `firelands_auth`.`schema_migrations`
WHERE `migration` IN (
  '24_world_creature_tables.sql',
  '25_world_creature_classlevelstats.sql',
  '29_world_creature_template_modelids.sql',
  'firelands_world_creature_ref_import.sql',
  '31_world_creature_template_gossip_menu_id.sql',
  '52_world_creature_template_spells.sql',
  '59_world_creature_trigger_phase_fix.sql'
);
