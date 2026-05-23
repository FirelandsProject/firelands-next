CREATE DATABASE IF NOT EXISTS `firelands_world`;
USE `firelands_world`;

-- World database usually contains static data like items, quests, etc.
-- We'll start with a small table for version tracking.
CREATE TABLE IF NOT EXISTS `version` (
  `core_version` varchar(120) NOT NULL,
  `db_version` varchar(120) DEFAULT NULL,
  PRIMARY KEY (`core_version`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO `version` (`core_version`, `db_version`) VALUES ('Firelands 4.3.4.15595', 'Initial Schema 2026-04-11');

-- Gossip menu definitions (maps menu ID to default NPC text).
CREATE TABLE IF NOT EXISTS `gossip_menu` (
  `MenuID` int unsigned NOT NULL DEFAULT '0',
  `TextID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` smallint NOT NULL DEFAULT '0',
  PRIMARY KEY (`MenuID`, `TextID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Gossip menu options (individual items shown in a gossip menu).
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

-- Gossip menu option action data (menu chaining, POI markers).
CREATE TABLE IF NOT EXISTS `gossip_menu_option_action` (
  `MenuId` int unsigned NOT NULL DEFAULT '0',
  `OptionIndex` int unsigned NOT NULL DEFAULT '0',
  `ActionMenuId` int unsigned NOT NULL DEFAULT '0',
  `ActionPoiId` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`MenuId`, `OptionIndex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Gossip menu option box (password/money confirmation popups).
CREATE TABLE IF NOT EXISTS `gossip_menu_option_box` (
  `MenuId` int unsigned NOT NULL DEFAULT '0',
  `OptionIndex` int unsigned NOT NULL DEFAULT '0',
  `BoxCoded` tinyint unsigned NOT NULL DEFAULT '0',
  `BoxMoney` int unsigned NOT NULL DEFAULT '0',
  `BoxText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BoxBroadcastTextId` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`MenuId`, `OptionIndex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- NPC gossip page text (`npc_text`; used by CMSG_NPC_TEXT_QUERY).
CREATE TABLE IF NOT EXISTS `npc_text` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `text0_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `text0_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BroadcastTextID0` int NOT NULL DEFAULT '0',
  `lang0` tinyint unsigned NOT NULL DEFAULT '0',
  `Probability0` float NOT NULL DEFAULT '0',
  `EmoteDelay0_0` smallint unsigned NOT NULL DEFAULT '0',
  `Emote0_0` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteDelay0_1` smallint unsigned NOT NULL DEFAULT '0',
  `Emote0_1` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteDelay0_2` smallint unsigned NOT NULL DEFAULT '0',
  `Emote0_2` smallint unsigned NOT NULL DEFAULT '0',
  `text1_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `text1_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BroadcastTextID1` int NOT NULL DEFAULT '0',
  `lang1` tinyint unsigned NOT NULL DEFAULT '0',
  `Probability1` float NOT NULL DEFAULT '0',
  `EmoteDelay1_0` smallint unsigned NOT NULL DEFAULT '0',
  `Emote1_0` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteDelay1_1` smallint unsigned NOT NULL DEFAULT '0',
  `Emote1_1` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteDelay1_2` smallint unsigned NOT NULL DEFAULT '0',
  `Emote1_2` smallint unsigned NOT NULL DEFAULT '0',
  `text2_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `text2_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BroadcastTextID2` int NOT NULL DEFAULT '0',
  `lang2` tinyint unsigned NOT NULL DEFAULT '0',
  `Probability2` float NOT NULL DEFAULT '0',
  `EmoteDelay2_0` smallint unsigned NOT NULL DEFAULT '0',
  `Emote2_0` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteDelay2_1` smallint unsigned NOT NULL DEFAULT '0',
  `Emote2_1` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteDelay2_2` smallint unsigned NOT NULL DEFAULT '0',
  `Emote2_2` smallint unsigned NOT NULL DEFAULT '0',
  `text3_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `text3_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BroadcastTextID3` int NOT NULL DEFAULT '0',
  `lang3` tinyint unsigned NOT NULL DEFAULT '0',
  `Probability3` float NOT NULL DEFAULT '0',
  `EmoteDelay3_0` smallint unsigned NOT NULL DEFAULT '0',
  `Emote3_0` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteDelay3_1` smallint unsigned NOT NULL DEFAULT '0',
  `Emote3_1` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteDelay3_2` smallint unsigned NOT NULL DEFAULT '0',
  `Emote3_2` smallint unsigned NOT NULL DEFAULT '0',
  `text4_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `text4_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BroadcastTextID4` int NOT NULL DEFAULT '0',
  `lang4` tinyint unsigned NOT NULL DEFAULT '0',
  `Probability4` float NOT NULL DEFAULT '0',
  `EmoteDelay4_0` smallint unsigned NOT NULL DEFAULT '0',
  `Emote4_0` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteDelay4_1` smallint unsigned NOT NULL DEFAULT '0',
  `Emote4_1` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteDelay4_2` smallint unsigned NOT NULL DEFAULT '0',
  `Emote4_2` smallint unsigned NOT NULL DEFAULT '0',
  `text5_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `text5_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BroadcastTextID5` int NOT NULL DEFAULT '0',
  `lang5` tinyint unsigned NOT NULL DEFAULT '0',
  `Probability5` float NOT NULL DEFAULT '0',
  `EmoteDelay5_0` smallint unsigned NOT NULL DEFAULT '0',
  `Emote5_0` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteDelay5_1` smallint unsigned NOT NULL DEFAULT '0',
  `Emote5_1` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteDelay5_2` smallint unsigned NOT NULL DEFAULT '0',
  `Emote5_2` smallint unsigned NOT NULL DEFAULT '0',
  `text6_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `text6_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BroadcastTextID6` int NOT NULL DEFAULT '0',
  `lang6` tinyint unsigned NOT NULL DEFAULT '0',
  `Probability6` float NOT NULL DEFAULT '0',
  `EmoteDelay6_0` smallint unsigned NOT NULL DEFAULT '0',
  `Emote6_0` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteDelay6_1` smallint unsigned NOT NULL DEFAULT '0',
  `Emote6_1` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteDelay6_2` smallint unsigned NOT NULL DEFAULT '0',
  `Emote6_2` smallint unsigned NOT NULL DEFAULT '0',
  `text7_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `text7_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BroadcastTextID7` int NOT NULL DEFAULT '0',
  `lang7` tinyint unsigned NOT NULL DEFAULT '0',
  `Probability7` float NOT NULL DEFAULT '0',
  `EmoteDelay7_0` smallint unsigned NOT NULL DEFAULT '0',
  `Emote7_0` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteDelay7_1` smallint unsigned NOT NULL DEFAULT '0',
  `Emote7_1` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteDelay7_2` smallint unsigned NOT NULL DEFAULT '0',
  `Emote7_2` smallint unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Minimal quest templates for gossip quest lines (full quest system is separate).
CREATE TABLE IF NOT EXISTS `quest_template` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `QuestLevel` smallint NOT NULL DEFAULT '1',
  `LogTitle` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Flags` int unsigned NOT NULL DEFAULT '0',
  `AllowableClasses` int unsigned NOT NULL DEFAULT '0',
  `AllowableRaces` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Creature → quest offered in gossip (`SMSG_GOSSIP_MESSAGE` quest block).
CREATE TABLE IF NOT EXISTS `creature_queststarter` (
  `id` mediumint unsigned NOT NULL DEFAULT '0' COMMENT 'Creature entry',
  `quest` int unsigned NOT NULL DEFAULT '0' COMMENT 'Quest Identifier',
  PRIMARY KEY (`id`, `quest`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
