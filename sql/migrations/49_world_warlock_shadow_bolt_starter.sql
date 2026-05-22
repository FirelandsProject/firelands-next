-- Warlock level-1 Shadow Bolt (686): teach at create, stop class-tab skill wire.
USE `firelands_world`;

INSERT IGNORE INTO `playercreateinfo_spell` (`raceMask`, `classMask`, `spellId`)
VALUES (0, 256, 686);

DELETE FROM `playercreateinfo_skill`
WHERE `classMask` = 256 AND `skillId` IN (354, 355, 593, 802);
