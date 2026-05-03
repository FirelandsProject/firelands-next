-- Phase D: optional per-spell direct health delta on hit (server MVP; replace with real effect loading later).
CREATE DATABASE IF NOT EXISTS `firelands_world`;
USE `firelands_world`;

ALTER TABLE `spell_dbc`
  ADD COLUMN `MvpDirectHealthDelta` int DEFAULT NULL
  COMMENT 'NULL = none; negative damage, positive heal (SpellDefinition.directHealthEffectBasePoints)'
  AFTER `OvSchoolMask`;
