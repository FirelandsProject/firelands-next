-- reference combat spells on creature_template (spell1..spell8).
-- Populate via tools/sql/import_ref_creature_data.py after re-importing templates.

USE `firelands_world`;

SET @exist_spell1 :=
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
   WHERE TABLE_SCHEMA = DATABASE()
     AND TABLE_NAME = 'creature_template'
     AND COLUMN_NAME = 'spell1');

SET @fl_sql := IF(@exist_spell1 = 0,
  'ALTER TABLE `creature_template`
     ADD COLUMN `spell1` int unsigned NOT NULL DEFAULT ''0'' AFTER `flags_extra`,
     ADD COLUMN `spell2` int unsigned NOT NULL DEFAULT ''0'' AFTER `spell1`,
     ADD COLUMN `spell3` int unsigned NOT NULL DEFAULT ''0'' AFTER `spell2`,
     ADD COLUMN `spell4` int unsigned NOT NULL DEFAULT ''0'' AFTER `spell3`,
     ADD COLUMN `spell5` int unsigned NOT NULL DEFAULT ''0'' AFTER `spell4`,
     ADD COLUMN `spell6` int unsigned NOT NULL DEFAULT ''0'' AFTER `spell5`,
     ADD COLUMN `spell7` int unsigned NOT NULL DEFAULT ''0'' AFTER `spell6`,
     ADD COLUMN `spell8` int unsigned NOT NULL DEFAULT ''0'' AFTER `spell7`',
  'SELECT 1');

PREPARE _fl_m52_p FROM @fl_sql;
EXECUTE _fl_m52_p;
DEALLOCATE PREPARE _fl_m52_p;
