-- Per-level creature stats (`creature_classlevelstats`) + template level range.
-- JDBC-safe: seed uses INSERT…SELECT + recursive CTE (MySQL 8+), no procedures.

USE `firelands_world`;

CREATE TABLE IF NOT EXISTS `creature_classlevelstats` (
  `level` tinyint unsigned NOT NULL,
  `class` tinyint unsigned NOT NULL COMMENT 'Creature UnitClass (matches creature_template.unit_class; 0 = warrior fallback in core)',
  `basemana` int unsigned NOT NULL DEFAULT '0',
  `attackpower` smallint NOT NULL DEFAULT '0',
  `rangedattackpower` smallint NOT NULL DEFAULT '0',
  `basehealth` int unsigned NOT NULL DEFAULT '100',
  `comment` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`level`,`class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT IGNORE INTO `creature_classlevelstats`
  (`level`, `class`, `basemana`, `attackpower`, `rangedattackpower`, `basehealth`, `comment`)
WITH RECURSIVE `lvl` (`n`) AS (
  SELECT 1 AS `n`
  UNION ALL
  SELECT `n` + 1 FROM `lvl` WHERE `n` < 85
),
`cls` (`c`) AS (
  SELECT 1 AS `c`
  UNION ALL
  SELECT `c` + 1 FROM `cls` WHERE `c` < 11
)
SELECT
  `lvl`.`n`,
  `cls`.`c`,
  CASE WHEN `cls`.`c` IN (1, 4) THEN 0 ELSE LEAST(60000, 80 + `lvl`.`n` * 45) END,
  CAST(5 + `lvl`.`n` * 3 AS SIGNED),
  CAST(
    CASE WHEN `cls`.`c` IN (3, 8, 9, 11) THEN 5 + `lvl`.`n` * 3
         ELSE (5 + `lvl`.`n` * 3) DIV 2 END AS SIGNED),
  LEAST(4000000, 45 + `lvl`.`n` * 18 + `cls`.`c` * 4),
  'Firelands seed'
FROM `lvl` CROSS JOIN `cls`;

SET @exist_minlevel :=
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
   WHERE TABLE_SCHEMA = DATABASE()
     AND TABLE_NAME = 'creature_template'
     AND COLUMN_NAME = 'minlevel');

SET @sql_minmax := IF(@exist_minlevel = 0,
  'ALTER TABLE `creature_template`
     ADD COLUMN `minlevel` tinyint unsigned NOT NULL DEFAULT ''1'' AFTER `VerifiedBuild`,
     ADD COLUMN `maxlevel` tinyint unsigned NOT NULL DEFAULT ''1'' AFTER `minlevel`',
  'SELECT 1');

PREPARE stmt_minmax FROM @sql_minmax;
EXECUTE stmt_minmax;
DEALLOCATE PREPARE stmt_minmax;

UPDATE `creature_template`
SET `minlevel` = 3, `maxlevel` = 5, `unit_class` = 1
WHERE `entry` = 6;

INSERT IGNORE INTO `creature` (
  `guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `phaseUseFlags`,
  `PhaseId`, `PhaseGroup`, `terrainSwapMap`, `modelid`, `equipment_id`,
  `position_x`, `position_y`, `position_z`, `orientation`,
  `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curHealthPct`,
  `MovementType`, `npcflag`, `unit_flags`, `unit_flags2`, `unit_flags3`,
  `ScriptName`, `StringId`, `VerifiedBuild`
) VALUES (
  9000001, 6, 0, 9, 9, '0', 0,
  0, 0, -1, 2576, 0,
  -8949.0, -132.0, 83.5, 5.2,
  120, 0, 0, 100,
  0, NULL, NULL, NULL, NULL,
  '', NULL, 0
);
