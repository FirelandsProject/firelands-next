-- Mage level-1 Fireball (133): teach at create, stop class-tab skill wire.
USE `firelands_world`;

INSERT IGNORE INTO `playercreateinfo_spell` (`raceMask`, `classMask`, `spellId`)
VALUES (0, 128, 133);

DELETE FROM `playercreateinfo_skill`
WHERE `classMask` = 128 AND `skillId` IN (6, 8, 237, 799);
