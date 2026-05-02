-- Repair installs where `12_character_account_data.sql` failed before `characters` existed.
CREATE DATABASE IF NOT EXISTS `firelands_characters`;
USE `firelands_characters`;

CREATE TABLE IF NOT EXISTS `character_account_data` (
  `guid` int(10) unsigned NOT NULL,
  `type` tinyint(3) unsigned NOT NULL,
  `time` int(10) unsigned NOT NULL DEFAULT '0',
  `data` blob NOT NULL,
  PRIMARY KEY (`guid`,`type`),
  CONSTRAINT `fk_character_account_data_guid` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
