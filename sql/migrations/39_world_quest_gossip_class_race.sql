-- Add class/race masks to minimal `quest_template` (from ref `quest_template_addon`).
-- Idempotent on MySQL 8.0 (no `ADD COLUMN IF NOT EXISTS`).
-- Re-import data: python3 tools/sql/import_ref_quest_gossip.py

USE `firelands_world`;

SET @exist_allowable_classes :=
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
   WHERE TABLE_SCHEMA = DATABASE()
     AND TABLE_NAME = 'quest_template'
     AND COLUMN_NAME = 'AllowableClasses');

SET @fl_sql := IF(@exist_allowable_classes = 0,
  'ALTER TABLE `quest_template`
     ADD COLUMN `AllowableClasses` int unsigned NOT NULL DEFAULT ''0''
     AFTER `Flags`',
  'SELECT 1');

PREPARE _fl_m39_classes FROM @fl_sql;
EXECUTE _fl_m39_classes;
DEALLOCATE PREPARE _fl_m39_classes;

SET @exist_allowable_races :=
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
   WHERE TABLE_SCHEMA = DATABASE()
     AND TABLE_NAME = 'quest_template'
     AND COLUMN_NAME = 'AllowableRaces');

SET @fl_sql := IF(@exist_allowable_races = 0,
  'ALTER TABLE `quest_template`
     ADD COLUMN `AllowableRaces` int unsigned NOT NULL DEFAULT ''0''
     AFTER `AllowableClasses`',
  'SELECT 1');

PREPARE _fl_m39_races FROM @fl_sql;
EXECUTE _fl_m39_races;
DEALLOCATE PREPARE _fl_m39_races;
