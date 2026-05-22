-- Migration: per-character action bar layout (Cataclysm 4.3.4)
-- Target: `firelands_characters`

CREATE DATABASE IF NOT EXISTS `firelands_characters`;
USE `firelands_characters`;

CREATE TABLE IF NOT EXISTS `character_action` (
  `guid` int unsigned NOT NULL,
  `spec` tinyint unsigned NOT NULL DEFAULT '0',
  `button` tinyint unsigned NOT NULL,
  `action` int unsigned NOT NULL DEFAULT '0',
  `type` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`, `spec`, `button`),
  KEY `idx_guid` (`guid`),
  CONSTRAINT `fk_character_action_guid` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
