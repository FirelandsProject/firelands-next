-- Paladin level-1 Crusader Strike (35395): teach at create, stop class-tab skill wire.
-- Yellow "trainable" in spellbook was client SkillLineAbility without server-known spell.
USE `firelands_world`;

INSERT IGNORE INTO `playercreateinfo_spell` (`raceMask`, `classMask`, `spellId`)
VALUES (0, 2, 35395);

DELETE FROM `playercreateinfo_skill`
WHERE `classMask` = 2 AND `skillId` IN (184, 267, 594, 800);
