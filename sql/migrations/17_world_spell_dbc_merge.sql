-- spell_dbc: SQL layer merged with client Spell.dbc (TCPP parity + server-only column).
-- Adds PowerType for optional overrides; NULL means inherit from Spell.dbc at load time.
-- Idempotent: duplicate column (1060) is ignored by DatabaseMigrator.
CREATE DATABASE IF NOT EXISTS `firelands_world`;
USE `firelands_world`;

ALTER TABLE `spell_dbc` ADD COLUMN `PowerType` int unsigned DEFAULT NULL COMMENT 'Optional power type override, NULL uses DBC' AFTER `SpellTargetRestrictionsId`;
