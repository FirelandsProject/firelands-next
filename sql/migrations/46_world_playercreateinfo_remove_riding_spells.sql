-- Remove riding / flying trainer spells incorrectly present in starter spell data.
USE `firelands_world`;

DELETE FROM `playercreateinfo_spell` WHERE `spellId` IN (
  33388, 33391, 34090, 34091, 54197, 90265, 90267,
  40120, 33943, 86470, 86530
);
