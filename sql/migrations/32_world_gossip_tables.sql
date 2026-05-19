-- Gossip schema (DDL only). Data: migration 35 (`python3 tools/sql/import_ref_gossip.py`).
-- JDBC-safe: CREATE IF NOT EXISTS.

USE `firelands_world`;

CREATE TABLE IF NOT EXISTS `gossip_menu` (
  `MenuID` int unsigned NOT NULL DEFAULT '0',
  `TextID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` smallint NOT NULL DEFAULT '0',
  PRIMARY KEY (`MenuID`, `TextID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `gossip_menu_option` (
  `MenuId` int unsigned NOT NULL DEFAULT '0',
  `OptionIndex` int unsigned NOT NULL DEFAULT '0',
  `OptionIcon` tinyint unsigned NOT NULL DEFAULT '0',
  `OptionText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `OptionBroadcastTextId` int unsigned NOT NULL DEFAULT '0',
  `OptionType` int unsigned NOT NULL DEFAULT '0',
  `OptionNpcflag` bigint unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` smallint NOT NULL DEFAULT '0',
  PRIMARY KEY (`MenuId`, `OptionIndex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `gossip_menu_option_action` (
  `MenuId` int unsigned NOT NULL DEFAULT '0',
  `OptionIndex` int unsigned NOT NULL DEFAULT '0',
  `ActionMenuId` int unsigned NOT NULL DEFAULT '0',
  `ActionPoiId` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`MenuId`, `OptionIndex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `gossip_menu_option_box` (
  `MenuId` int unsigned NOT NULL DEFAULT '0',
  `OptionIndex` int unsigned NOT NULL DEFAULT '0',
  `BoxCoded` tinyint unsigned NOT NULL DEFAULT '0',
  `BoxMoney` int unsigned NOT NULL DEFAULT '0',
  `BoxText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BoxBroadcastTextId` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`MenuId`, `OptionIndex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
