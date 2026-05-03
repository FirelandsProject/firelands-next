-- Wipe creature-related rows in firelands_world and unmark creature migrations so the
-- next auth or world server startup re-runs those SQL files (DatabaseMigrator).
--
-- Requires a MySQL user that can modify both `firelands_world` and `firelands_auth`.
-- Docker example (repo root):
--   docker compose exec -T db mysql -uroot -proot < tools/sql/reset_world_creatures_and_migrations.sql
--
-- After this: restart `world` (or `auth`; either runs the same migrator on `sql/`).
-- If you use `import_ref_creature_data.py`, output `30_world_creature_ref_import.sql`
-- before restarting so templates include `modelid1`..`modelid4` (migration 29 adds columns).

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
