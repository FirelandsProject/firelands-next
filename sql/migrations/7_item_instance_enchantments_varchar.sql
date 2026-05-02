-- MariaDB: TEXT/BLOB cannot use DEFAULT; VARCHAR can. Upgrades older `item_instance` DDL.
CREATE DATABASE IF NOT EXISTS `firelands_characters`;
USE `firelands_characters`;

ALTER TABLE `item_instance`
  MODIFY COLUMN `enchantments` varchar(4096) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '';
