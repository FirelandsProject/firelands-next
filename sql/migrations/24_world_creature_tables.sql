-- TrinityCore-aligned creature schema for Firelands Next (world DB).
-- Behaviour is Lua-driven; `smart_scripts` is intentionally omitted.
-- JDBC-safe: no DELIMITER / stored procedures (DatabaseMigrator splits on ';').
--
-- Legacy installs from migration 23 (only entry/name/subname): conditional rebuild via
-- backup table + DROP + CREATE + restore rows.

USE `firelands_world`;

SELECT CASE
         WHEN EXISTS (SELECT 1 FROM information_schema.TABLES
                      WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'creature_template')
              AND NOT EXISTS (SELECT 1 FROM information_schema.COLUMNS
                              WHERE TABLE_SCHEMA = DATABASE()
                                AND TABLE_NAME = 'creature_template'
                                AND COLUMN_NAME = 'faction')
              THEN 1 ELSE 0 END
INTO @fl_m24_legacy_expand;

SET @fl_sql := IF(@fl_m24_legacy_expand = 1,
  'DROP TABLE IF EXISTS `_fl_m24_ct_backup`',
  'SELECT 1');
PREPARE _fl_m24_p FROM @fl_sql;
EXECUTE _fl_m24_p;
DEALLOCATE PREPARE _fl_m24_p;

SET @fl_sql := IF(@fl_m24_legacy_expand = 1,
  'CREATE TABLE `_fl_m24_ct_backup` (`entry` int unsigned NOT NULL, `name` varchar(255) NOT NULL, `subname` varchar(255) NOT NULL, PRIMARY KEY (`entry`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci',
  'SELECT 1');
PREPARE _fl_m24_p FROM @fl_sql;
EXECUTE _fl_m24_p;
DEALLOCATE PREPARE _fl_m24_p;

SET @fl_sql := IF(@fl_m24_legacy_expand = 1,
  'INSERT INTO `_fl_m24_ct_backup` (`entry`,`name`,`subname`) SELECT `entry`,`name`,`subname` FROM `creature_template`',
  'SELECT 1');
PREPARE _fl_m24_p FROM @fl_sql;
EXECUTE _fl_m24_p;
DEALLOCATE PREPARE _fl_m24_p;

SET @fl_sql := IF(@fl_m24_legacy_expand = 1,
  'DROP TABLE `creature_template`',
  'SELECT 1');
PREPARE _fl_m24_p FROM @fl_sql;
EXECUTE _fl_m24_p;
DEALLOCATE PREPARE _fl_m24_p;

CREATE TABLE IF NOT EXISTS `creature_template` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `KillCredit1` int unsigned NOT NULL DEFAULT '0',
  `KillCredit2` int unsigned NOT NULL DEFAULT '0',
  `name` mediumtext,
  `femaleName` mediumtext,
  `subname` mediumtext,
  `TitleAlt` mediumtext,
  `IconName` varchar(64) DEFAULT NULL,
  `RequiredExpansion` int NOT NULL DEFAULT '0',
  `VignetteID` int NOT NULL DEFAULT '0',
  `faction` smallint unsigned NOT NULL DEFAULT '0',
  `npcflag` bigint unsigned NOT NULL DEFAULT '0',
  `speed_walk` float NOT NULL DEFAULT '1',
  `speed_run` float NOT NULL DEFAULT '1.14286',
  `scale` float NOT NULL DEFAULT '1',
  `Classification` tinyint unsigned NOT NULL DEFAULT '0',
  `dmgschool` tinyint NOT NULL DEFAULT '0',
  `BaseAttackTime` int unsigned NOT NULL DEFAULT '0',
  `RangeAttackTime` int unsigned NOT NULL DEFAULT '0',
  `BaseVariance` float NOT NULL DEFAULT '1',
  `RangeVariance` float NOT NULL DEFAULT '1',
  `unit_class` tinyint unsigned NOT NULL DEFAULT '0',
  `unit_flags` int unsigned NOT NULL DEFAULT '0',
  `unit_flags2` int unsigned NOT NULL DEFAULT '0',
  `unit_flags3` int unsigned NOT NULL DEFAULT '0',
  `family` int NOT NULL DEFAULT '0',
  `trainer_class` tinyint unsigned NOT NULL DEFAULT '0',
  `type` tinyint unsigned NOT NULL DEFAULT '0',
  `VehicleId` int unsigned NOT NULL DEFAULT '0',
  `AIName` varchar(64) NOT NULL DEFAULT '',
  `MovementType` tinyint unsigned NOT NULL DEFAULT '0',
  `ExperienceModifier` float NOT NULL DEFAULT '1',
  `RacialLeader` tinyint unsigned NOT NULL DEFAULT '0',
  `movementId` int unsigned NOT NULL DEFAULT '0',
  `WidgetSetID` int NOT NULL DEFAULT '0',
  `WidgetSetUnitConditionID` int NOT NULL DEFAULT '0',
  `RegenHealth` tinyint unsigned NOT NULL DEFAULT '1',
  `CreatureImmunitiesId` int NOT NULL DEFAULT '0',
  `flags_extra` int unsigned NOT NULL DEFAULT '0',
  `ScriptName` varchar(64) NOT NULL DEFAULT '',
  `StringId` varchar(64) DEFAULT NULL,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`entry`),
  KEY `idx_creature_template_name` (`name`(64))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='Creature definitions (AI via Lua; no smart_scripts)';

SET @fl_sql := IF(@fl_m24_legacy_expand = 1,
  'INSERT INTO `creature_template` (`entry`,`name`,`subname`) SELECT `entry`,`name`,`subname` FROM `_fl_m24_ct_backup`',
  'SELECT 1');
PREPARE _fl_m24_p FROM @fl_sql;
EXECUTE _fl_m24_p;
DEALLOCATE PREPARE _fl_m24_p;

SET @fl_sql := IF(@fl_m24_legacy_expand = 1,
  'DROP TABLE `_fl_m24_ct_backup`',
  'SELECT 1');
PREPARE _fl_m24_p FROM @fl_sql;
EXECUTE _fl_m24_p;
DEALLOCATE PREPARE _fl_m24_p;

CREATE TABLE IF NOT EXISTS `creature` (
  `guid` bigint unsigned NOT NULL DEFAULT '0',
  `id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Creature Identifier',
  `map` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'Map Identifier',
  `zoneId` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'Zone Identifier',
  `areaId` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'Area Identifier',
  `spawnDifficulties` varchar(100) NOT NULL DEFAULT '0',
  `phaseUseFlags` tinyint unsigned NOT NULL DEFAULT '0',
  `PhaseId` int DEFAULT '0',
  `PhaseGroup` int DEFAULT '0',
  `terrainSwapMap` int NOT NULL DEFAULT '-1',
  `modelid` int unsigned NOT NULL DEFAULT '0',
  `equipment_id` tinyint NOT NULL DEFAULT '0',
  `position_x` float NOT NULL DEFAULT '0',
  `position_y` float NOT NULL DEFAULT '0',
  `position_z` float NOT NULL DEFAULT '0',
  `orientation` float NOT NULL DEFAULT '0',
  `spawntimesecs` int unsigned NOT NULL DEFAULT '120',
  `wander_distance` float NOT NULL DEFAULT '0',
  `currentwaypoint` int unsigned NOT NULL DEFAULT '0',
  `curHealthPct` int unsigned NOT NULL DEFAULT '100',
  `MovementType` tinyint unsigned NOT NULL DEFAULT '0',
  `npcflag` bigint unsigned DEFAULT NULL,
  `unit_flags` int unsigned DEFAULT NULL,
  `unit_flags2` int unsigned DEFAULT NULL,
  `unit_flags3` int unsigned DEFAULT NULL,
  `ScriptName` varchar(64) NOT NULL DEFAULT '',
  `StringId` varchar(64) DEFAULT NULL,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`),
  KEY `idx_map` (`map`),
  KEY `idx_id` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='Creature spawns (world placement)';

CREATE TABLE IF NOT EXISTS `creature_addon` (
  `guid` bigint unsigned NOT NULL DEFAULT '0',
  `PathId` int unsigned NOT NULL DEFAULT '0',
  `mount` int unsigned NOT NULL DEFAULT '0',
  `MountCreatureID` int unsigned NOT NULL DEFAULT '0',
  `StandState` tinyint unsigned NOT NULL DEFAULT '0',
  `AnimTier` tinyint unsigned NOT NULL DEFAULT '0',
  `VisFlags` tinyint unsigned NOT NULL DEFAULT '0',
  `SheathState` tinyint unsigned NOT NULL DEFAULT '1',
  `PvPFlags` tinyint unsigned NOT NULL DEFAULT '0',
  `emote` int unsigned NOT NULL DEFAULT '0',
  `aiAnimKit` smallint unsigned NOT NULL DEFAULT '0',
  `movementAnimKit` smallint unsigned NOT NULL DEFAULT '0',
  `meleeAnimKit` smallint unsigned NOT NULL DEFAULT '0',
  `visibilityDistanceType` tinyint unsigned NOT NULL DEFAULT '0',
  `auras` mediumtext,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `creature_template_addon` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `PathId` int unsigned NOT NULL DEFAULT '0',
  `mount` int unsigned NOT NULL DEFAULT '0',
  `MountCreatureID` int unsigned NOT NULL DEFAULT '0',
  `StandState` tinyint unsigned NOT NULL DEFAULT '0',
  `AnimTier` tinyint unsigned NOT NULL DEFAULT '0',
  `VisFlags` tinyint unsigned NOT NULL DEFAULT '0',
  `SheathState` tinyint unsigned NOT NULL DEFAULT '1',
  `PvPFlags` tinyint unsigned NOT NULL DEFAULT '0',
  `emote` int unsigned NOT NULL DEFAULT '0',
  `aiAnimKit` smallint unsigned NOT NULL DEFAULT '0',
  `movementAnimKit` smallint unsigned NOT NULL DEFAULT '0',
  `meleeAnimKit` smallint unsigned NOT NULL DEFAULT '0',
  `visibilityDistanceType` tinyint unsigned NOT NULL DEFAULT '0',
  `auras` mediumtext,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `creature_template` (`entry`, `name`, `subname`) VALUES
  (6, 'Kobold Vermin', 'Monster'),
  (2575, 'Harlan Bagley', 'Baker'),
  (35176, 'Stormwind Mage', 'Portal Trainer')
ON DUPLICATE KEY UPDATE
  `name` = VALUES(`name`),
  `subname` = VALUES(`subname`);
