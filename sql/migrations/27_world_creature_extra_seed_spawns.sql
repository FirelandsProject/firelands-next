-- Spawns for creature_template seeds from migration 24 that had no `creature` rows.
-- Migration 25 only inserted the Kobold (entry 6, guid 9000001).

USE `firelands_world`;

UPDATE `creature_template`
SET `minlevel` = 25, `maxlevel` = 25, `unit_class` = 1
WHERE `entry` = 2575;

UPDATE `creature_template`
SET `minlevel` = 35, `maxlevel` = 35, `unit_class` = 8
WHERE `entry` = 35176;

INSERT IGNORE INTO `creature` (
  `guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `phaseUseFlags`,
  `PhaseId`, `PhaseGroup`, `terrainSwapMap`, `modelid`, `equipment_id`,
  `position_x`, `position_y`, `position_z`, `orientation`,
  `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curHealthPct`,
  `MovementType`, `npcflag`, `unit_flags`, `unit_flags2`, `unit_flags3`,
  `ScriptName`, `StringId`, `VerifiedBuild`
) VALUES
(
  9000002, 2575, 0, 9, 9, '0', 0,
  0, 0, -1, 0, 0,
  -8945.0, -128.0, 83.5, 5.0,
  120, 0, 0, 100,
  0, NULL, NULL, NULL, NULL,
  '', NULL, 0
),
(
  9000003, 35176, 0, 9, 9, '0', 0,
  0, 0, -1, 0, 0,
  -8942.0, -136.0, 83.5, 5.4,
  120, 0, 0, 100,
  0, NULL, NULL, NULL, NULL,
  '', NULL, 0
);
