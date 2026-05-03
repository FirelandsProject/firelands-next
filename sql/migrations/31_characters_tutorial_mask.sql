-- Persist UI tutorial completion bitmask (`SMSG_TUTORIAL_FLAGS` / Trinity-style ints).
CREATE DATABASE IF NOT EXISTS `firelands_characters`;
USE `firelands_characters`;

ALTER TABLE `characters`
  ADD COLUMN `tutorial0` int unsigned NOT NULL DEFAULT 0 AFTER `live_power1`,
  ADD COLUMN `tutorial1` int unsigned NOT NULL DEFAULT 0 AFTER `tutorial0`,
  ADD COLUMN `tutorial2` int unsigned NOT NULL DEFAULT 0 AFTER `tutorial1`,
  ADD COLUMN `tutorial3` int unsigned NOT NULL DEFAULT 0 AFTER `tutorial2`,
  ADD COLUMN `tutorial4` int unsigned NOT NULL DEFAULT 0 AFTER `tutorial3`,
  ADD COLUMN `tutorial5` int unsigned NOT NULL DEFAULT 0 AFTER `tutorial4`,
  ADD COLUMN `tutorial6` int unsigned NOT NULL DEFAULT 0 AFTER `tutorial5`,
  ADD COLUMN `tutorial7` int unsigned NOT NULL DEFAULT 0 AFTER `tutorial6`;
