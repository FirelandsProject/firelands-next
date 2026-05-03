CREATE DATABASE IF NOT EXISTS `firelands_characters`;
USE `firelands_characters`;

-- Idempotent: never DROP on startup (migrations re-run would wipe data).
CREATE TABLE IF NOT EXISTS `characters` (
  `guid` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `account` int(10) unsigned NOT NULL DEFAULT '0',
  `name` varchar(12) NOT NULL DEFAULT '',
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `gender` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `skin` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `face` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `hairStyle` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `hairColor` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `facialHair` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `outfitId` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `equipmentCache` mediumtext NULL,
  `level` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `zoneId` smallint(5) unsigned NOT NULL DEFAULT '0',
  `mapId` smallint(5) unsigned NOT NULL DEFAULT '0',
  `x` float NOT NULL DEFAULT '0',
  `y` float NOT NULL DEFAULT '0',
  `z` float NOT NULL DEFAULT '0',
  `orientation` float NOT NULL DEFAULT '0',
  `guildId` int(10) unsigned NOT NULL DEFAULT '0',
  `characterFlags` int(10) unsigned NOT NULL DEFAULT '0',
  `customizationFlags` int(10) unsigned NOT NULL DEFAULT '0',
  `firstLogin` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `money` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`),
  KEY `idx_account` (`account`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `character_account_data` (
  `guid` int(10) unsigned NOT NULL,
  `type` tinyint(3) unsigned NOT NULL,
  `time` int(10) unsigned NOT NULL DEFAULT '0',
  `data` blob NOT NULL,
  PRIMARY KEY (`guid`,`type`),
  CONSTRAINT `fk_character_account_data_guid` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `character_spell` (
  `guid` int unsigned NOT NULL,
  `spell` int unsigned NOT NULL,
  PRIMARY KEY (`guid`, `spell`),
  KEY `idx_guid` (`guid`),
  CONSTRAINT `fk_character_spell_guid` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `gm_ticket` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `account_id` int unsigned NOT NULL,
  `character_guid` int unsigned NOT NULL,
  `status` tinyint unsigned NOT NULL DEFAULT '0',
  `category` tinyint unsigned NOT NULL DEFAULT '0',
  `need_more_help` tinyint unsigned NOT NULL DEFAULT '0',
  `message` text NOT NULL,
  `gm_response` text NULL,
  `map_id` smallint unsigned NOT NULL DEFAULT '0',
  `pos_x` float NOT NULL DEFAULT '0',
  `pos_y` float NOT NULL DEFAULT '0',
  `pos_z` float NOT NULL DEFAULT '0',
  `assigned_account_id` int unsigned NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `assigned_at` timestamp NULL DEFAULT NULL,
  `closed_at` timestamp NULL DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_character_open` (`character_guid`, `status`),
  KEY `idx_queue` (`status`, `created_at`),
  KEY `idx_assigned` (`assigned_account_id`, `status`),
  CONSTRAINT `fk_gm_ticket_character` FOREIGN KEY (`character_guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `mail` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `receiver_guid` int unsigned NOT NULL,
  `sender_guid` int unsigned NOT NULL DEFAULT '0',
  `subject` varchar(200) NOT NULL DEFAULT 'Item delivery',
  `body` text,
  `deliver_time` int unsigned NOT NULL DEFAULT '0',
  `expire_time` int unsigned NOT NULL DEFAULT '0',
  `checked` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `idx_receiver` (`receiver_guid`),
  CONSTRAINT `fk_mail_receiver` FOREIGN KEY (`receiver_guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `mail_items` (
  `mail_id` bigint unsigned NOT NULL,
  `item_guid` int unsigned NOT NULL,
  `receiver_guid` int unsigned NOT NULL,
  PRIMARY KEY (`item_guid`),
  KEY `idx_mail` (`mail_id`),
  CONSTRAINT `fk_mail_items_mail` FOREIGN KEY (`mail_id`) REFERENCES `mail` (`id`) ON DELETE CASCADE,
  CONSTRAINT `fk_mail_items_receiver` FOREIGN KEY (`receiver_guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
