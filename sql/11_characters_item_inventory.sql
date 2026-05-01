CREATE DATABASE IF NOT EXISTS `firelands_characters`;
USE `firelands_characters`;

-- Starter items persisted like Trinity/FirelandsCore (`item_instance` + `character_inventory`).
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
  `enchantments` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
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
