-- Merged characters migrations (Firelands Next)
-- Execution order: 4 -> 6 -> 7 -> 11 -> 16 -> 18 -> z_ensure

-- === 6_characters_add_outfitid.sql ===
CREATE DATABASE IF NOT EXISTS `firelands_characters`;
USE `firelands_characters`;

SET @exist :=
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
   WHERE TABLE_SCHEMA = DATABASE()
     AND TABLE_NAME = 'characters'
     AND COLUMN_NAME = 'outfitId');

SET @sqlstmt := IF(@exist = 0,
  'ALTER TABLE `characters` ADD COLUMN `outfitId` tinyint unsigned NOT NULL DEFAULT 0 AFTER `facialHair`',
  'SELECT 1');

PREPARE stmt FROM @sqlstmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- === 7_characters_equipment_cache.sql ===
SET @exist :=
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
   WHERE TABLE_SCHEMA = DATABASE()
     AND TABLE_NAME = 'characters'
     AND COLUMN_NAME = 'equipmentCache');

SET @sqlstmt := IF(@exist = 0,
  'ALTER TABLE `characters` ADD COLUMN `equipmentCache` mediumtext NULL AFTER `outfitId`',
  'SELECT 1');

PREPARE stmt FROM @sqlstmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- === 11_characters_item_inventory.sql ===
CREATE TABLE IF NOT EXISTS `item_instance` (
  `guid` int unsigned NOT NULL AUTO_INCREMENT,
  `itemEntry` int unsigned NOT NULL DEFAULT '0',
  `owner_guid` int unsigned NOT NULL DEFAULT '0',
  `creatorGuid` int unsigned NOT NULL DEFAULT '0',
  `giftCreatorGuid` int unsigned NOT NULL DEFAULT '0',
  `count` int unsigned NOT NULL DEFAULT '1',
  `duration` int NOT NULL DEFAULT '0',
  `charges` tinytext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `flags` int unsigned NOT NULL DEFAULT '0',
  `enchantments` varchar(4096) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `randomPropertyType` tinyint unsigned NOT NULL DEFAULT '0',
  `randomPropertyId` int unsigned NOT NULL DEFAULT '0',
  `durability` smallint unsigned NOT NULL DEFAULT '0',
  `creationTime` int unsigned NOT NULL DEFAULT '0',
  `text` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`guid`),
  KEY `idx_owner_guid` (`owner_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_inventory` (
  `guid` int unsigned NOT NULL DEFAULT '0',
  `bag` int unsigned NOT NULL DEFAULT '0',
  `slot` tinyint unsigned NOT NULL DEFAULT '0',
  `item` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`item`),
  UNIQUE KEY `guid` (`guid`,`bag`,`slot`),
  KEY `idx_guid` (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- === 16_gameplay_money_spells_account_lock.sql (characters portion) ===
ALTER TABLE `firelands_characters`.`characters`
  ADD COLUMN `money` int unsigned NOT NULL DEFAULT '0'
  AFTER `firstLogin`;

CREATE TABLE IF NOT EXISTS `firelands_characters`.`character_spell` (
  `guid` int unsigned NOT NULL,
  `spell` int unsigned NOT NULL,
  PRIMARY KEY (`guid`, `spell`),
  KEY `idx_guid` (`guid`),
  CONSTRAINT `fk_character_spell_guid` FOREIGN KEY (`guid`) REFERENCES `firelands_characters`.`characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- === 18_gm_ticket.sql ===
CREATE TABLE IF NOT EXISTS `firelands_characters`.`gm_ticket` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `account_id` int unsigned NOT NULL COMMENT 'Player account that opened the ticket',
  `character_guid` int unsigned NOT NULL,
  `status` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '0=open 1=assigned 2=gm_answered 3=closed_resolved 4=closed_abandoned 5=closed_staff',
  `category` tinyint unsigned NOT NULL DEFAULT '0',
  `need_more_help` tinyint unsigned NOT NULL DEFAULT '0',
  `message` text NOT NULL,
  `gm_response` text NULL COMMENT 'Last GM reply shown to player when status is gm_answered',
  `map_id` smallint unsigned NOT NULL DEFAULT '0',
  `pos_x` float NOT NULL DEFAULT '0',
  `pos_y` float NOT NULL DEFAULT '0',
  `pos_z` float NOT NULL DEFAULT '0',
  `assigned_account_id` int unsigned NULL COMMENT 'Staff account.id currently handling the ticket',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `assigned_at` timestamp NULL DEFAULT NULL,
  `closed_at` timestamp NULL DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_character_open` (`character_guid`, `status`),
  KEY `idx_queue` (`status`, `created_at`),
  KEY `idx_assigned` (`assigned_account_id`, `status`),
  CONSTRAINT `fk_gm_ticket_character` FOREIGN KEY (`character_guid`) REFERENCES `firelands_characters`.`characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- === z_ensure_character_account_data.sql ===
CREATE TABLE IF NOT EXISTS `character_account_data` (
  `guid` int(10) unsigned NOT NULL,
  `type` tinyint(3) unsigned NOT NULL,
  `time` int(10) unsigned NOT NULL DEFAULT '0',
  `data` blob NOT NULL,
  PRIMARY KEY (`guid`, `type`),
  CONSTRAINT `fk_character_account_data_guid` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;