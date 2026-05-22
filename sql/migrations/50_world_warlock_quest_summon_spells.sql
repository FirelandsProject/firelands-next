-- Warlock demon summons are quest-gated (e.g. Summon Imp 688 via starter chain), not at create.
USE `firelands_world`;

DELETE FROM `playercreateinfo_spell`
WHERE `classMask` = 256
  AND `spellId` IN (688, 687, 689, 691, 693, 697, 698, 702, 710, 712);
