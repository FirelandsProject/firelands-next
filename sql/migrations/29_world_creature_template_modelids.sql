-- Trinity stores creature visuals on `creature_template` (`modelid1`..`modelid4`).
-- Spawn row `creature.modelid` is often 0 (meaning "use template"). Without these
-- columns the core falls back to a single placeholder display for every NPC.
-- JDBC-safe conditional DDL.

USE `firelands_world`;

SET @exist_modelid1 :=
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
   WHERE TABLE_SCHEMA = DATABASE()
     AND TABLE_NAME = 'creature_template'
     AND COLUMN_NAME = 'modelid1');

SET @fl_sql := IF(@exist_modelid1 = 0,
  'ALTER TABLE `creature_template`
     ADD COLUMN `modelid1` int unsigned NOT NULL DEFAULT ''0'' AFTER `KillCredit2`,
     ADD COLUMN `modelid2` int unsigned NOT NULL DEFAULT ''0'' AFTER `modelid1`,
     ADD COLUMN `modelid3` int unsigned NOT NULL DEFAULT ''0'' AFTER `modelid2`,
     ADD COLUMN `modelid4` int unsigned NOT NULL DEFAULT ''0'' AFTER `modelid3`',
  'SELECT 1');

PREPARE _fl_m29_p FROM @fl_sql;
EXECUTE _fl_m29_p;
DEALLOCATE PREPARE _fl_m29_p;
