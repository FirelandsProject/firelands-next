-- Migration: persist which action bars are visible (PLAYER_FIELD_BYTES toggles byte)
-- Target: `firelands_characters`

CREATE DATABASE IF NOT EXISTS `firelands_characters`;
USE `firelands_characters`;

ALTER TABLE `characters`
  ADD COLUMN `actionBarToggles` tinyint unsigned NOT NULL DEFAULT '255'
  AFTER `tutorial7`;
