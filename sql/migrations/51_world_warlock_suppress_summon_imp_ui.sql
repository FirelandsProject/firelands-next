-- Warlock: quest-gated summons + no class-tab skills (stops yellow trainable UI).
-- Also run 49_world_warlock_shadow_bolt_starter.sql if not applied yet.
USE `firelands_world`;

INSERT IGNORE INTO `playercreateinfo_spell` (`raceMask`, `classMask`, `spellId`)
VALUES (0, 256, 686);

DELETE FROM `playercreateinfo_spell`
WHERE `classMask` = 256
  AND `spellId` IN (688, 687, 689, 691, 693, 697, 698, 702, 710, 712);

DELETE FROM `playercreateinfo_skill`
WHERE `classMask` = 256 AND `skillId` IN (354, 355, 593, 802);
