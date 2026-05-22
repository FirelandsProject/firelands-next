-- Rogue level-1 Sinister Strike (1752): teach at create, stop class-tab skill wire.
USE `firelands_world`;

INSERT IGNORE INTO `playercreateinfo_spell` (`raceMask`, `classMask`, `spellId`)
VALUES (0, 8, 1752);

DELETE FROM `playercreateinfo_skill`
WHERE `classMask` = 8 AND `skillId` IN (38, 39, 253, 797);
