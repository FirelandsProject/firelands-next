-- Minimal mail storage for GM item overflow (full backpack).
-- Full SMSG_MAIL_LIST / mailbox UI is not implemented yet; rows persist for a future mail layer.

CREATE DATABASE IF NOT EXISTS `firelands_characters`;
USE `firelands_characters`;

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
