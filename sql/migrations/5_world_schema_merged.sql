-- Merged world migrations (Firelands Next)
-- Execution order: lexicographic in DatabaseMigrator; this bundle mirrors 2..16 + 17 + z_ensure + 24 + 25 + 26

-- === 2_playercreateinfo.sql + 4 + 5 + backfill ===
CREATE DATABASE IF NOT EXISTS `firelands_world`;
USE `firelands_world`;

CREATE TABLE IF NOT EXISTS `playercreateinfo_stats` (
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `str` int(10) unsigned NOT NULL DEFAULT '0',
  `agi` int(10) unsigned NOT NULL DEFAULT '0',
  `sta` int(10) unsigned NOT NULL DEFAULT '0',
  `intel` int(10) unsigned NOT NULL DEFAULT '0',
  `spi` int(10) unsigned NOT NULL DEFAULT '0',
  `maxHealth` int(10) unsigned NOT NULL DEFAULT '100',
  `maxMana` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`race`, `class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `playercreateinfo_spell` (
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `spellId` int(10) unsigned NOT NULL,
  PRIMARY KEY (`race`, `class`, `spellId`),
  KEY `idx_class` (`class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `playercreateinfo_skill` (
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `skillId` smallint(5) unsigned NOT NULL,
  `rank` smallint(5) unsigned NOT NULL DEFAULT '0',
  `maxRank` smallint(5) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`race`, `class`, `skillId`),
  KEY `idx_class` (`class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `playercreateinfo_faction` (
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `reputationListId` smallint(5) unsigned NOT NULL,
  `flags` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `standing` int(10) NOT NULL DEFAULT '0',
  PRIMARY KEY (`race`, `class`, `reputationListId`),
  KEY `idx_class` (`class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `playercreateinfo_reputation` (
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `minLevel` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `maxLevel` tinyint(3) unsigned NOT NULL DEFAULT '255',
  `factionId` int(10) unsigned NOT NULL,
  `flags` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `standing` int(10) NOT NULL DEFAULT '0',
  PRIMARY KEY (`race`, `class`, `minLevel`, `maxLevel`, `factionId`),
  KEY `idx_race_class` (`race`, `class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Stats seed (race=0 wildcard)
INSERT IGNORE INTO `playercreateinfo_stats` (`race`, `class`, `str`, `agi`, `sta`, `intel`, `spi`, `maxHealth`, `maxMana`) VALUES
  (0, 1, 23, 20, 22, 18, 19, 100, 0),
  (0, 2, 22, 18, 21, 20, 20, 100, 100),
  (0, 3, 18, 23, 20, 19, 20, 100, 0),
  (0, 4, 19, 24, 20, 18, 19, 100, 0),
  (0, 5, 17, 18, 19, 24, 23, 100, 100),
  (0, 6, 23, 18, 22, 18, 18, 100, 0),
  (0, 7, 19, 19, 20, 22, 21, 100, 100),
  (0, 8, 17, 18, 19, 25, 22, 100, 100),
  (0, 9, 17, 18, 20, 24, 20, 100, 100),
  (0,11, 18, 19, 20, 22, 22, 100, 100);

-- Language skills
INSERT IGNORE INTO `playercreateinfo_skill` (`race`, `class`, `skillId`, `rank`, `maxRank`) VALUES
  (0, 0, 98, 300, 300),
  (0, 0, 109, 300, 300);

-- Starter spells (class=0 wildcard)
INSERT IGNORE INTO `playercreateinfo_spell` (`race`, `class`, `spellId`) VALUES
  (0, 0, 668), (0, 0, 669),
  (0, 1, 2457), (0, 1, 71), (0, 1, 78), (0, 1, 100), (0, 1, 6673), (0, 1, 772), (0, 1, 3127), (0, 1, 34428),
  (0, 2, 465), (0, 2, 635), (0, 2, 20154), (0, 2, 20271), (0, 2, 19740), (0, 2, 498), (0, 2, 633), (0, 2, 82242),
  (0, 3, 75), (0, 3, 13165), (0, 3, 1978), (0, 3, 3044), (0, 3, 56641), (0, 3, 781), (0, 3, 1130), (0, 3, 2973),
  (0, 4, 1784), (0, 4, 2098), (0, 4, 53), (0, 4, 1752), (0, 4, 921), (0, 4, 1766), (0, 4, 1776), (0, 4, 82245),
  (0, 5, 585), (0, 5, 589), (0, 5, 2061), (0, 5, 17), (0, 5, 139), (0, 5, 2050), (0, 5, 8092),
  (0, 6, 48263), (0, 6, 45524), (0, 6, 49998), (0, 6, 47528), (0, 6, 48721), (0, 6, 45529), (0, 6, 48792),
  (0, 7, 331), (0, 7, 8042), (0, 7, 8017), (0, 7, 8050), (0, 7, 324), (0, 7, 51730), (0, 7, 5185), (0, 7, 52127),
  (0, 8, 116), (0, 8, 133), (0, 8, 2136), (0, 8, 1459), (0, 8, 130), (0, 8, 1953), (0, 8, 118),
  (0, 9, 172), (0, 9, 348), (0, 9, 687), (0, 9, 1454), (0, 9, 5782), (0, 9, 980), (0, 9, 603),
  (0,11, 8921), (0,11, 5185), (0,11, 774), (0,11, 768), (0,11, 1126), (0,11, 339), (0,11, 467);

-- Starter reputations
INSERT IGNORE INTO `playercreateinfo_reputation` (`race`, `class`, `minLevel`, `maxLevel`, `factionId`, `flags`, `standing`) VALUES
  (1, 0, 1, 255, 72,  1, 0),
  (1, 0, 1, 255, 47,  1, 0),
  (1, 0, 1, 255, 69,  1, 0),
  (1, 0, 1, 255, 54,  1, 0),
  (1, 0, 1, 255, 930, 1, 0),
  (2, 0, 1, 255, 76,  1, 0),
  (2, 0, 1, 255, 81,  1, 0),
  (2, 0, 1, 255, 68,  1, 0),
  (2, 0, 1, 255, 530, 1, 0),
  (2, 0, 1, 255, 911, 1, 0);

-- === 5_playercreateinfo_visual_items.sql ===
CREATE TABLE IF NOT EXISTS `playercreateinfo_visual_items` (
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `gender` tinyint(3) unsigned NOT NULL DEFAULT '2',
  `outfitId` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `slot` tinyint(3) unsigned NOT NULL,
  `invType` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `displayId` int(10) unsigned NOT NULL DEFAULT '0',
  `displayEnchantId` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`race`, `class`, `gender`, `outfitId`, `slot`),
  KEY `idx_class` (`class`),
  KEY `idx_race_class` (`race`, `class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- === 8_world_item_proto_playercreateinfo_item.sql ===
CREATE TABLE IF NOT EXISTS `playercreateinfo_item` (
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `itemid` int(10) unsigned NOT NULL DEFAULT '0',
  `amount` tinyint(4) NOT NULL DEFAULT '1',
  PRIMARY KEY (`race`,`class`,`itemid`),
  KEY `idx_race_class` (`race`,`class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- === 9_firelands_world_item_template.sql ===
-- Parity with reference implementation `data/sql/base/db_hotfixes/item.sql` (DB2 Item.db2):
--   ID->entry, ClassID->class, SubclassID->subclass, SoundOverrideSubclassID,
--   Material, DisplayInfoID->displayid, InventoryType, SheatheType, VerifiedBuild.
-- `BuyCount` is server-side (not in DB2 item); kept for inventory/equip logic.
CREATE TABLE IF NOT EXISTS `item_template` (
  `entry` int unsigned NOT NULL,
  `class` tinyint unsigned NOT NULL DEFAULT 0,
  `subclass` tinyint unsigned NOT NULL DEFAULT 0,
  `sound_override_subclass` int NOT NULL DEFAULT 0,
  `Material` int NOT NULL DEFAULT 0,
  `displayid` int unsigned NOT NULL DEFAULT 0,
  `InventoryType` tinyint unsigned NOT NULL DEFAULT 0,
  `SheatheType` tinyint unsigned NOT NULL DEFAULT 0,
  `verified_build` smallint NOT NULL DEFAULT 0,
  `BuyCount` int NOT NULL DEFAULT 1,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- === 10_playercreateinfo_spawn.sql ===
CREATE TABLE IF NOT EXISTS `playercreateinfo` (
  `race` tinyint unsigned NOT NULL DEFAULT '0',
  `class` tinyint unsigned NOT NULL DEFAULT '0',
  `map` smallint unsigned NOT NULL DEFAULT '0',
  `zone` int unsigned NOT NULL DEFAULT '0',
  `position_x` float NOT NULL DEFAULT '0',
  `position_y` float NOT NULL DEFAULT '0',
  `position_z` float NOT NULL DEFAULT '0',
  `orientation` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`race`,`class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO `playercreateinfo` (`race`, `class`, `map`, `zone`, `position_x`, `position_y`, `position_z`, `orientation`) VALUES (1,1,0,9,-8914.57,-133.909,80.5378,5.13806),
(1,2,0,9,-8914.57,-133.909,80.5378,5.13806),
(1,3,0,9,-8914.57,-133.909,80.5378,5.13806),
(1,4,0,9,-8914.57,-133.909,80.5378,5.13806),
(1,5,0,9,-8914.57,-133.909,80.5378,5.13806),
(1,6,609,4298,2355.84,-5664.77,426.028,3.93485),
(1,8,0,9,-8914.57,-133.909,80.5378,5.13806),
(1,9,0,9,-8914.57,-133.909,80.5378,5.13806),
(2,1,1,14,-618.518,-4251.67,38.718,4.72222),
(2,3,1,14,-618.518,-4251.67,38.718,4.72222),
(2,4,1,14,-618.518,-4251.67,38.718,4.72222),
(2,6,609,4298,2358.44,-5666.9,426.023,3.93485),
(2,7,1,14,-618.518,-4251.67,38.718,4.72222),
(2,8,1,14,-618.518,-4251.67,38.718,4.72222),
(2,9,1,14,-618.518,-4251.67,38.718,4.72222),
(3,1,0,1,-6240.32,331.033,382.758,6.17716),
(3,2,0,1,-6240.32,331.033,382.758,6.17716),
(3,3,0,1,-6240.32,331.033,382.758,6.17716),
(3,4,0,1,-6240.32,331.033,382.758,6.17716),
(3,5,0,1,-6240.32,331.033,382.758,6.17716),
(3,6,609,4298,2358.44,-5666.9,426.023,3.93485),
(3,7,0,1,-6240.32,331.033,382.758,6.17716),
(3,8,0,1,-6240.32,331.033,382.758,6.17716),
(3,9,0,1,-6240.32,331.033,382.758,6.17716),
(4,1,1,141,10311.3,832.463,1326.41,5.69632),
(4,3,1,141,10311.3,832.463,1326.41,5.69632),
(4,4,1,141,10311.3,832.463,1326.41,5.69632),
(4,5,1,141,10311.3,832.463,1326.41,5.69632),
(4,6,609,4298,2356.21,-5662.21,426.026,3.93485),
(4,8,1,141,10311.3,832.463,1326.41,5.69632),
(4,11,1,141,10311.3,832.463,1326.41,5.69632),
(5,1,0,5692,1699.85,1706.56,135.928,4.88839),
(5,3,0,5692,1699.85,1706.56,135.928,4.88839),
(5,4,0,5692,1699.85,1706.56,135.928,4.88839),
(5,5,0,5692,1699.85,1706.56,135.928,4.88839),
(5,6,609,4298,2356.21,-5662.21,426.026,3.93485),
(5,8,0,5692,1699.85,1706.56,135.928,4.88839),
(5,9,0,5692,1699.85,1706.56,135.928,4.88839),
(6,1,1,221,-2915.55,-257.347,59.2693,0.302378),
(6,2,1,221,-2915.55,-257.347,59.2693,0.302378),
(6,3,1,221,-2915.55,-257.347,59.2693,0.302378),
(6,5,1,221,-2915.55,-257.347,59.2693,0.302378),
(6,6,609,4298,2358.17,-5663.21,426.027,3.93485),
(6,7,1,221,-2915.55,-257.347,59.2693,0.302378),
(7,1,1,2158,2779.67,-767.128,0.649066,0),
(7,2,1,2158,2779.67,-767.128,0.649066,0),
(7,3,1,2158,2779.67,-767.128,0.649066,0),
(7,4,1,2158,2779.67,-767.128,0.649066,0),
(7,5,1,2158,2779.67,-767.128,0.649066,0),
(7,6,609,4298,2356.21,-5662.21,426.026,3.93485),
(7,7,1,2158,2779.67,-767.128,0.649066,0),
(7,8,1,2158,2779.67,-767.128,0.649066,0),
(7,9,1,2158,2779.67,-767.128,0.649066,0),
(8,1,1,5691,-1171.45,-5263.65,0.847728,5.78945),
(8,2,1,5691,-1171.45,-5263.65,0.847728,5.78945),
(8,3,1,5691,-1171.45,-5263.65,0.847728,5.78945),
(8,4,1,5691,-1171.45,-5263.65,0.847728,5.78945),
(8,5,1,5691,-1171.45,-5263.65,0.847728,5.78945),
(8,6,609,4298,2356.21,-5662.21,426.026,3.93485),
(8,7,1,5691,-1171.45,-5263.65,0.847728,5.78945),
(8,8,1,5691,-1171.45,-5263.65,0.847728,5.78945),
(8,9,1,5691,-1171.45,-5263.65,0.847728,5.78945),
(10,1,1,2158,2779.67,-767.128,0.649066,0),
(10,2,1,2158,2779.67,-767.128,0.649066,0),
(10,3,1,2158,2779.67,-767.128,0.649066,0),
(10,4,1,2158,2779.67,-767.128,0.649066,0),
(10,5,1,2158,2779.67,-767.128,0.649066,0),
(10,6,609,4298,2356.21,-5662.21,426.026,3.93485),
(10,7,1,2158,2779.67,-767.128,0.649066,0),
(10,8,1,2158,2779.67,-767.128,0.649066,0),
(10,9,1,2158,2779.67,-767.128,0.649066,0),
(11,1,1,141,10311.3,832.463,1326.41,5.69632),
(11,2,1,141,10311.3,832.463,1326.41,5.69632),
(11,3,1,141,10311.3,832.463,1326.41,5.69632),
(11,4,1,141,10311.3,832.463,1326.41,5.69632),
(11,5,1,141,10311.3,832.463,1326.41,5.69632),
(11,6,609,4298,2356.21,-5662.21,426.026,3.93485),
(11,7,1,141,10311.3,832.463,1326.41,5.69632),
(11,8,1,141,10311.3,832.463,1326.41,5.69632),
(11,9,1,141,10311.3,832.463,1326.41,5.69632),
(22,1,1,141,10311.3,832.463,1326.41,5.69632),
(22,2,1,141,10311.3,832.463,1326.41,5.69632),
(22,3,1,141,10311.3,832.463,1326.41,5.69632),
(22,4,1,141,10311.3,832.463,1326.41,5.69632),
(22,5,1,141,10311.3,832.463,1326.41,5.69632),
(22,6,609,4298,2356.21,-5662.21,426.026,3.93485),
(22,7,1,141,10311.3,832.463,1326.41,5.69632),
(22,8,1,141,10311.3,832.463,1326.41,5.69632),
(22,9,1,141,10311.3,832.463,1326.41,5.69632);

-- === 13_playercreateinfo_spell_languages.sql ===
INSERT IGNORE INTO `playercreateinfo_spell` (`race`, `class`, `spellId`) VALUES
  (1, 0, 668), (2, 0, 669), (3, 0, 672), (3, 0, 668), (4, 0, 671), (4, 0, 668),
  (5, 0, 17737), (5, 0, 669), (6, 0, 670), (6, 0, 669), (7, 0, 7340), (7, 0, 668),
  (8, 0, 7341), (8, 0, 669), (9, 0, 69269), (9, 0, 669), (10, 0, 813), (10, 0, 669),
  (11, 0, 29932), (11, 0, 668), (22, 0, 69270), (22, 0, 668);

-- === 14_remove_invalid_starter_spell_ids.sql ===
DELETE FROM `playercreateinfo_spell` WHERE `spellId` IN (86470, 86471, 86473, 86475, 86478);

-- === 17_player_class_and_race_stats.sql ===
CREATE TABLE IF NOT EXISTS `player_classlevelstats` (
  `class` tinyint unsigned NOT NULL,
  `level` tinyint unsigned NOT NULL,
  `str` smallint unsigned NOT NULL DEFAULT 0,
  `agi` smallint unsigned NOT NULL DEFAULT 0,
  `sta` smallint unsigned NOT NULL DEFAULT 0,
  `inte` smallint unsigned NOT NULL DEFAULT 0,
  `spi` smallint unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`class`,`level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `player_racestats` (
  `race` tinyint unsigned NOT NULL,
  `str` smallint NOT NULL DEFAULT 0,
  `agi` smallint NOT NULL DEFAULT 0,
  `sta` smallint NOT NULL DEFAULT 0,
  `inte` smallint NOT NULL DEFAULT 0,
  `spi` smallint NOT NULL DEFAULT 0,
  PRIMARY KEY (`race`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO `player_classlevelstats` (`class`, `level`, `str`, `agi`, `sta`, `inte`, `spi`) VALUES
  (1,1,23,20,22,20,21), (2,1,23,20,22,20,22), (3,1,22,21,22,20,21),
  (4,1,23,21,21,20,21), (5,1,17,22,22,22,23), (6,1,25,19,22,20,22),
  (7,1,22,21,22,20,22), (8,1,17,22,22,23,23), (9,1,21,21,22,23,23), (11,1,22,20,22,22,23);

INSERT IGNORE INTO `player_racestats` (`race`, `str`, `agi`, `sta`, `inte`, `spi`) VALUES
  (1,0,0,0,0,0), (2,3,-3,3,-3,0), (3,0,0,1,0,0), (4,-4,2,0,0,0),
  (5,0,0,0,0,0), (6,1,0,1,0,0), (7,-5,2,0,3,0), (8,1,2,0,0,0),
  (9,0,0,0,0,0), (10,0,0,0,0,0), (11,0,0,0,2,0), (22,0,0,0,0,0);

-- === 15_world_player_xp_for_level.sql ===
CREATE TABLE IF NOT EXISTS `player_xp_for_level` (
  `Level` tinyint unsigned NOT NULL,
  `Experience` int unsigned NOT NULL,
  PRIMARY KEY (`Level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `player_xp_for_level` (`Level`, `Experience`) VALUES
(1,400),(2,900),(3,1400),(4,2100),(5,2800),(6,3600),(7,4500),(8,5400),(9,6500),(10,7600),
(11,8700),(12,9800),(13,11000),(14,12300),(15,13600),(16,15000),(17,16400),(18,17800),(19,19300),(20,20800),
(21,22400),(22,24000),(23,25500),(24,27200),(25,28900),(26,30500),(27,32200),(28,33900),(29,36300),(30,38800),
(31,41600),(32,44600),(33,48000),(34,51400),(35,55000),(36,58700),(37,62400),(38,66200),(39,70200),(40,74300),
(41,78500),(42,82800),(43,87100),(44,91600),(45,96300),(46,101000),(47,105800),(48,110700),(49,115700),(50,120900),
(51,126100),(52,131500),(53,137000),(54,142500),(55,148200),(56,154000),(57,159900),(58,165800),(59,172000),(60,290000),
(61,317000),(62,349000),(63,386000),(64,428000),(65,475000),(66,527000),(67,585000),(68,648000),(69,717000),(70,1523800),
(71,1539600),(72,1555700),(73,1571800),(74,1587900),(75,1604200),(76,1620700),(77,1637400),(78,1653900),(79,1670800),
(80,1686300),(81,2121500),(82,4004000),(83,5203400),(84,9165100)
ON DUPLICATE KEY UPDATE `Experience` = VALUES(`Experience`);

-- === 16_world_spell_tables.sql ===
CREATE TABLE IF NOT EXISTS `spell_area` (
  `spell` int unsigned NOT NULL DEFAULT 0,
  `area` int unsigned NOT NULL DEFAULT 0,
  `quest_start` int unsigned NOT NULL DEFAULT 0,
  `quest_end` int unsigned NOT NULL DEFAULT 0,
  `aura_spell` int NOT NULL DEFAULT 0,
  `racemask` int unsigned NOT NULL DEFAULT 0,
  `gender` tinyint unsigned NOT NULL DEFAULT 2,
  `flags` tinyint unsigned NOT NULL DEFAULT 3,
  `quest_start_status` int NOT NULL DEFAULT 64,
  `quest_end_status` int NOT NULL DEFAULT 11,
  PRIMARY KEY (`spell`,`area`,`quest_start`,`aura_spell`,`racemask`,`gender`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spell_bonus_data` (
  `entry` int unsigned NOT NULL DEFAULT 0,
  `direct_bonus` float NOT NULL DEFAULT 0,
  `dot_bonus` float NOT NULL DEFAULT 0,
  `ap_bonus` float NOT NULL DEFAULT 0,
  `ap_dot_bonus` float NOT NULL DEFAULT 0,
  `comments` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spell_custom_attr` (
  `entry` int unsigned NOT NULL DEFAULT 0 COMMENT 'spell id',
  `attributes` int unsigned NOT NULL DEFAULT 0 COMMENT 'SpellCustomAttributes',
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='SpellInfo custom attributes';

CREATE TABLE IF NOT EXISTS `spell_dbc` (
  `Id` int unsigned NOT NULL,
  `Attributes` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx2` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx3` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx4` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx5` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx6` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx7` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx8` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx9` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx10` int unsigned NOT NULL DEFAULT 0,
  `CastingTimeIndex` int unsigned NOT NULL DEFAULT 1,
  `DurationIndex` int unsigned NOT NULL DEFAULT 0,
  `RangeIndex` int unsigned NOT NULL DEFAULT 1,
  `SchoolMask` int unsigned NOT NULL DEFAULT 0,
  `SpellAuraOptionsId` int unsigned NOT NULL DEFAULT 0,
  `SpellCastingRequirementsId` int unsigned NOT NULL DEFAULT 0,
  `SpellCategoriesId` int unsigned NOT NULL DEFAULT 0,
  `SpellClassOptionsId` int unsigned NOT NULL DEFAULT 0,
  `SpellEquippedItemsId` int unsigned NOT NULL DEFAULT 0,
  `SpellInterruptsId` int unsigned NOT NULL DEFAULT 0,
  `SpellLevelsId` int unsigned NOT NULL DEFAULT 0,
  `SpellTargetRestrictionsId` int unsigned NOT NULL DEFAULT 0,
  `PowerType` int unsigned DEFAULT NULL COMMENT 'Optional power type override, NULL uses DBC',
  `OvAttributes` int unsigned DEFAULT NULL COMMENT 'Override SpellDefinition.attributes, NULL keeps DBC',
  `OvCastingTimeIndex` int unsigned DEFAULT NULL COMMENT 'Override castingTimeIndex, NULL keeps DBC',
  `OvDurationIndex` int unsigned DEFAULT NULL COMMENT 'Override durationIndex, NULL keeps DBC',
  `OvRangeIndex` int unsigned DEFAULT NULL COMMENT 'Override rangeIndex, NULL keeps DBC',
  `OvSchoolMask` int unsigned DEFAULT NULL COMMENT 'Override schoolMask, NULL keeps DBC',
  `SpellName` varchar(128) NOT NULL,
  PRIMARY KEY (`Id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Custom spell.dbc entries';

CREATE TABLE IF NOT EXISTS `spell_enchant_proc_data` (
  `entry` int unsigned NOT NULL,
  `customChance` int unsigned NOT NULL DEFAULT 0,
  `PPMChance` float NOT NULL DEFAULT 0,
  `procEx` int unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Spell enchant proc data';

CREATE TABLE IF NOT EXISTS `spell_group` (
  `id` int unsigned NOT NULL DEFAULT 0,
  `spell_id` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`,`spell_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Spell System';

CREATE TABLE IF NOT EXISTS `spell_group_stack_rules` (
  `group_id` int unsigned NOT NULL DEFAULT 0,
  `stack_rule` tinyint NOT NULL DEFAULT 0,
  PRIMARY KEY (`group_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spell_learn_spell` (
  `entry` int unsigned NOT NULL DEFAULT 0,
  `SpellID` int unsigned NOT NULL DEFAULT 0,
  `Active` tinyint unsigned NOT NULL DEFAULT 1,
  PRIMARY KEY (`entry`,`SpellID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Item System';

CREATE TABLE IF NOT EXISTS `spell_linked_spell` (
  `spell_trigger` int NOT NULL,
  `spell_effect` int NOT NULL DEFAULT 0,
  `type` tinyint unsigned NOT NULL DEFAULT 0,
  `comment` text NOT NULL,
  UNIQUE KEY `trigger_effect_type` (`spell_trigger`,`spell_effect`,`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Spell System';

CREATE TABLE IF NOT EXISTS `spell_loot_template` (
  `Entry` int unsigned NOT NULL DEFAULT 0,
  `Item` int unsigned NOT NULL DEFAULT 0,
  `Reference` int unsigned NOT NULL DEFAULT 0,
  `Chance` float NOT NULL DEFAULT 100,
  `QuestRequired` tinyint NOT NULL DEFAULT 0,
  `IsCurrency` tinyint NOT NULL DEFAULT 0,
  `LootMode` smallint unsigned NOT NULL DEFAULT 1,
  `GroupId` tinyint unsigned NOT NULL DEFAULT 0,
  `MinCount` tinyint unsigned NOT NULL DEFAULT 1,
  `MaxCount` tinyint unsigned NOT NULL DEFAULT 1,
  `Comment` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`Entry`,`Item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Loot System';

CREATE TABLE IF NOT EXISTS `spell_pet_auras` (
  `spell` int unsigned NOT NULL COMMENT 'dummy spell id',
  `effectId` tinyint unsigned NOT NULL DEFAULT 0,
  `pet` int unsigned NOT NULL DEFAULT 0 COMMENT 'pet id; 0 = all',
  `aura` int unsigned NOT NULL COMMENT 'pet aura id',
  PRIMARY KEY (`spell`,`effectId`,`pet`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spell_proc` (
  `SpellId` int NOT NULL DEFAULT 0,
  `SchoolMask` tinyint unsigned NOT NULL DEFAULT 0,
  `SpellFamilyName` smallint unsigned NOT NULL DEFAULT 0,
  `SpellFamilyMask0` int unsigned NOT NULL DEFAULT 0,
  `SpellFamilyMask1` int unsigned NOT NULL DEFAULT 0,
  `SpellFamilyMask2` int unsigned NOT NULL DEFAULT 0,
  `ProcFlags` int unsigned NOT NULL DEFAULT 0,
  `SpellTypeMask` int unsigned NOT NULL DEFAULT 0,
  `SpellPhaseMask` int unsigned NOT NULL DEFAULT 0,
  `HitMask` int unsigned NOT NULL DEFAULT 0,
  `AttributesMask` int unsigned NOT NULL DEFAULT 0,
  `DisableEffectsMask` int unsigned NOT NULL DEFAULT 0,
  `ProcsPerMinute` float NOT NULL DEFAULT 0,
  `Chance` float NOT NULL DEFAULT 0,
  `Cooldown` int unsigned NOT NULL DEFAULT 0,
  `Charges` tinyint unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`SpellId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spell_proc_event` (
  `entry` int NOT NULL DEFAULT 0,
  `SchoolMask` tinyint NOT NULL DEFAULT 0,
  `SpellFamilyName` smallint unsigned NOT NULL DEFAULT 0,
  `SpellFamilyMask0` int unsigned NOT NULL DEFAULT 0,
  `SpellFamilyMask1` int unsigned NOT NULL DEFAULT 0,
  `SpellFamilyMask2` int unsigned NOT NULL DEFAULT 0,
  `procFlags` int unsigned NOT NULL DEFAULT 0,
  `procEx` int unsigned NOT NULL DEFAULT 0,
  `ppmRate` float NOT NULL DEFAULT 0,
  `CustomChance` float NOT NULL DEFAULT 0,
  `Cooldown` int unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spell_ranks` (
  `first_spell_id` int unsigned NOT NULL DEFAULT 0,
  `spell_id` int unsigned NOT NULL DEFAULT 0,
  `rank` tinyint unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`first_spell_id`,`rank`),
  UNIQUE KEY `spell_id` (`spell_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Spell Rank Data';

CREATE TABLE IF NOT EXISTS `spell_required` (
  `spell_id` int NOT NULL DEFAULT 0,
  `req_spell` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`spell_id`,`req_spell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Spell Additinal Data';

CREATE TABLE IF NOT EXISTS `spell_script_names` (
  `spell_id` int NOT NULL,
  `ScriptName` varchar(64) NOT NULL,
  UNIQUE KEY `spell_id` (`spell_id`,`ScriptName`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spell_scripts` (
  `id` int unsigned NOT NULL DEFAULT 0,
  `effIndex` tinyint unsigned NOT NULL DEFAULT 0,
  `delay` int unsigned NOT NULL DEFAULT 0,
  `command` int unsigned NOT NULL DEFAULT 0,
  `datalong` int unsigned NOT NULL DEFAULT 0,
  `datalong2` int unsigned NOT NULL DEFAULT 0,
  `dataint` int NOT NULL DEFAULT 0,
  `x` float NOT NULL DEFAULT 0,
  `y` float NOT NULL DEFAULT 0,
  `z` float NOT NULL DEFAULT 0,
  `o` float NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spell_target_position` (
  `ID` int unsigned NOT NULL DEFAULT 0 COMMENT 'Identifier',
  `EffectIndex` tinyint unsigned NOT NULL DEFAULT 0,
  `MapID` smallint unsigned NOT NULL DEFAULT 0,
  `PositionX` float NOT NULL DEFAULT 0,
  `PositionY` float NOT NULL DEFAULT 0,
  `PositionZ` float NOT NULL DEFAULT 0,
  `Orientation` float NOT NULL DEFAULT 0,
  `VerifiedBuild` smallint DEFAULT 0,
  PRIMARY KEY (`ID`,`EffectIndex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Spell System';

CREATE TABLE IF NOT EXISTS `spell_threat` (
  `entry` int unsigned NOT NULL,
  `flatMod` int DEFAULT NULL,
  `pctMod` float NOT NULL DEFAULT 1 COMMENT 'threat multiplier for damage/healing',
  `apPctMod` float NOT NULL DEFAULT 0 COMMENT 'additional threat bonus from attack power',
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spelldifficulty_dbc` (
  `id` int unsigned NOT NULL DEFAULT 0,
  `spellid0` int unsigned NOT NULL DEFAULT 0,
  `spellid1` int unsigned NOT NULL DEFAULT 0,
  `spellid2` int unsigned NOT NULL DEFAULT 0,
  `spellid3` int unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spelleffect_dbc` (
  `Id` int unsigned NOT NULL,
  `Effect` int unsigned NOT NULL DEFAULT 0,
  `EffectAmplitude` float NOT NULL DEFAULT 0,
  `EffectAura` int unsigned NOT NULL DEFAULT 0,
  `EffectAuraPeriod` int unsigned NOT NULL DEFAULT 0,
  `EffectBasePoints` int NOT NULL DEFAULT 0,
  `EffectBonusCoefficient` float NOT NULL DEFAULT 0,
  `EffectChainAmplitude` float NOT NULL DEFAULT 0,
  `EffectChainTargets` int unsigned NOT NULL DEFAULT 0,
  `EffectDieSides` int NOT NULL DEFAULT 0,
  `EffectItemType` int unsigned NOT NULL DEFAULT 0,
  `EffectMechanic` int unsigned NOT NULL DEFAULT 0,
  `EffectMiscValue` int NOT NULL DEFAULT 0,
  `EffectMiscValueB` int NOT NULL DEFAULT 0,
  `EffectPointsPerResource` float NOT NULL DEFAULT 0,
  `EffectRadiusIndex` int unsigned NOT NULL DEFAULT 0,
  `EffectRadiusMaxIndex` int unsigned NOT NULL DEFAULT 0,
  `EffectRealPointsPerLevel` float NOT NULL DEFAULT 0,
  `EffectSpellClassMaskA` int unsigned NOT NULL DEFAULT 0,
  `EffectSpellClassMaskB` int unsigned NOT NULL DEFAULT 0,
  `EffectSpellClassMaskC` int unsigned NOT NULL DEFAULT 0,
  `EffectTriggerSpell` int unsigned NOT NULL DEFAULT 0,
  `EffectImplicitTargetA` int unsigned NOT NULL DEFAULT 0,
  `EffectImplicitTargetB` int unsigned NOT NULL DEFAULT 0,
  `SpellID` int unsigned NOT NULL,
  `EffectIndex` int unsigned NOT NULL DEFAULT 0,
  `Comment` varchar(128) NOT NULL,
  PRIMARY KEY (`Id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- spell_dbc: see sql/migrations/17_world_spell_dbc_merge.sql (PowerType ALTER for old DBs),
-- sql/migrations/18_world_spell_dbc_ov_columns.sql (Ov* ALTER for DBs created before Ov* in CREATE),
-- sql/migrations/22_world_spell_dbc_drop_mvp_columns.sql (drops legacy MVP columns if present).

-- === 22_world_spell_dbc_drop_mvp_columns.sql ===
SET @exist :=
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
   WHERE TABLE_SCHEMA = DATABASE()
     AND TABLE_NAME = 'spell_dbc'
     AND COLUMN_NAME = 'MvpManaCost');

SET @sqlstmt := IF(@exist > 0,
  'ALTER TABLE `spell_dbc` DROP COLUMN `MvpManaCost`',
  'SELECT 1');

PREPARE stmt FROM @sqlstmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @exist :=
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
   WHERE TABLE_SCHEMA = DATABASE()
     AND TABLE_NAME = 'spell_dbc'
     AND COLUMN_NAME = 'MvpDirectHealthDelta');

SET @sqlstmt := IF(@exist > 0,
  'ALTER TABLE `spell_dbc` DROP COLUMN `MvpDirectHealthDelta`',
  'SELECT 1');

PREPARE stmt FROM @sqlstmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;


-- === z_ensure_player_classlevelstats_seed.sql (idempotent seed) ===
INSERT IGNORE INTO `player_classlevelstats` (`class`, `level`, `str`, `agi`, `sta`, `inte`, `spi`) VALUES
  (1,1,23,20,22,20,21), (2,1,23,20,22,20,22), (3,1,22,21,22,20,21),
  (4,1,23,21,21,20,21), (5,1,17,22,22,22,23), (6,1,25,19,22,20,22),
  (7,1,22,21,22,20,22), (8,1,17,22,22,23,23), (9,1,21,21,22,23,23), (11,1,22,20,22,22,23);

INSERT IGNORE INTO `player_racestats` (`race`, `str`, `agi`, `sta`, `inte`, `spi`) VALUES
  (1,0,0,0,0,0), (2,3,-3,3,-3,0), (3,0,0,1,0,0), (4,-4,2,0,0,0),
  (5,0,0,0,0,0), (6,1,0,1,0,0), (7,-5,2,0,3,0), (8,1,2,0,0,0),
  (9,0,0,0,0,0), (10,0,0,0,0,0), (11,0,0,0,2,0), (22,0,0,0,0,0);


-- === 24_world_creature_tables.sql ===
-- Creature schema for Firelands Next (world DB).
-- Behaviour is Lua-driven; `smart_scripts` is intentionally omitted.
-- JDBC-safe: no DELIMITER / stored procedures (DatabaseMigrator splits on ';').
--
-- Legacy installs from migration 23 (only entry/name/subname): conditional rebuild via
-- backup table + DROP + CREATE + restore rows.

USE `firelands_world`;

SELECT CASE
         WHEN EXISTS (SELECT 1 FROM information_schema.TABLES
                      WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'creature_template')
              AND NOT EXISTS (SELECT 1 FROM information_schema.COLUMNS
                              WHERE TABLE_SCHEMA = DATABASE()
                                AND TABLE_NAME = 'creature_template'
                                AND COLUMN_NAME = 'faction')
              THEN 1 ELSE 0 END
INTO @fl_m24_legacy_expand;

SET @fl_sql := IF(@fl_m24_legacy_expand = 1,
  'DROP TABLE IF EXISTS `_fl_m24_ct_backup`',
  'SELECT 1');
PREPARE _fl_m24_p FROM @fl_sql;
EXECUTE _fl_m24_p;
DEALLOCATE PREPARE _fl_m24_p;

SET @fl_sql := IF(@fl_m24_legacy_expand = 1,
  'CREATE TABLE `_fl_m24_ct_backup` (`entry` int unsigned NOT NULL, `name` varchar(255) NOT NULL, `subname` varchar(255) NOT NULL, PRIMARY KEY (`entry`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci',
  'SELECT 1');
PREPARE _fl_m24_p FROM @fl_sql;
EXECUTE _fl_m24_p;
DEALLOCATE PREPARE _fl_m24_p;

SET @fl_sql := IF(@fl_m24_legacy_expand = 1,
  'INSERT INTO `_fl_m24_ct_backup` (`entry`,`name`,`subname`) SELECT `entry`,`name`,`subname` FROM `creature_template`',
  'SELECT 1');
PREPARE _fl_m24_p FROM @fl_sql;
EXECUTE _fl_m24_p;
DEALLOCATE PREPARE _fl_m24_p;

SET @fl_sql := IF(@fl_m24_legacy_expand = 1,
  'DROP TABLE `creature_template`',
  'SELECT 1');
PREPARE _fl_m24_p FROM @fl_sql;
EXECUTE _fl_m24_p;
DEALLOCATE PREPARE _fl_m24_p;

CREATE TABLE IF NOT EXISTS `creature_template` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `KillCredit1` int unsigned NOT NULL DEFAULT '0',
  `KillCredit2` int unsigned NOT NULL DEFAULT '0',
  `name` mediumtext,
  `femaleName` mediumtext,
  `subname` mediumtext,
  `TitleAlt` mediumtext,
  `IconName` varchar(64) DEFAULT NULL,
  `RequiredExpansion` int NOT NULL DEFAULT '0',
  `VignetteID` int NOT NULL DEFAULT '0',
  `faction` smallint unsigned NOT NULL DEFAULT '0',
  `npcflag` bigint unsigned NOT NULL DEFAULT '0',
  `speed_walk` float NOT NULL DEFAULT '1',
  `speed_run` float NOT NULL DEFAULT '1.14286',
  `scale` float NOT NULL DEFAULT '1',
  `Classification` tinyint unsigned NOT NULL DEFAULT '0',
  `dmgschool` tinyint NOT NULL DEFAULT '0',
  `BaseAttackTime` int unsigned NOT NULL DEFAULT '0',
  `RangeAttackTime` int unsigned NOT NULL DEFAULT '0',
  `BaseVariance` float NOT NULL DEFAULT '1',
  `RangeVariance` float NOT NULL DEFAULT '1',
  `unit_class` tinyint unsigned NOT NULL DEFAULT '0',
  `unit_flags` int unsigned NOT NULL DEFAULT '0',
  `unit_flags2` int unsigned NOT NULL DEFAULT '0',
  `unit_flags3` int unsigned NOT NULL DEFAULT '0',
  `family` int NOT NULL DEFAULT '0',
  `trainer_class` tinyint unsigned NOT NULL DEFAULT '0',
  `type` tinyint unsigned NOT NULL DEFAULT '0',
  `VehicleId` int unsigned NOT NULL DEFAULT '0',
  `AIName` varchar(64) NOT NULL DEFAULT '',
  `MovementType` tinyint unsigned NOT NULL DEFAULT '0',
  `ExperienceModifier` float NOT NULL DEFAULT '1',
  `RacialLeader` tinyint unsigned NOT NULL DEFAULT '0',
  `movementId` int unsigned NOT NULL DEFAULT '0',
  `WidgetSetID` int NOT NULL DEFAULT '0',
  `WidgetSetUnitConditionID` int NOT NULL DEFAULT '0',
  `RegenHealth` tinyint unsigned NOT NULL DEFAULT '1',
  `CreatureImmunitiesId` int NOT NULL DEFAULT '0',
  `flags_extra` int unsigned NOT NULL DEFAULT '0',
  `ScriptName` varchar(64) NOT NULL DEFAULT '',
  `StringId` varchar(64) DEFAULT NULL,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`entry`),
  KEY `idx_creature_template_name` (`name`(64))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='Creature definitions (AI via Lua; no smart_scripts)';

SET @fl_sql := IF(@fl_m24_legacy_expand = 1,
  'INSERT INTO `creature_template` (`entry`,`name`,`subname`) SELECT `entry`,`name`,`subname` FROM `_fl_m24_ct_backup`',
  'SELECT 1');
PREPARE _fl_m24_p FROM @fl_sql;
EXECUTE _fl_m24_p;
DEALLOCATE PREPARE _fl_m24_p;

SET @fl_sql := IF(@fl_m24_legacy_expand = 1,
  'DROP TABLE `_fl_m24_ct_backup`',
  'SELECT 1');
PREPARE _fl_m24_p FROM @fl_sql;
EXECUTE _fl_m24_p;
DEALLOCATE PREPARE _fl_m24_p;

CREATE TABLE IF NOT EXISTS `creature` (
  `guid` bigint unsigned NOT NULL DEFAULT '0',
  `id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Creature Identifier',
  `map` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'Map Identifier',
  `zoneId` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'Zone Identifier',
  `areaId` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'Area Identifier',
  `spawnDifficulties` varchar(100) NOT NULL DEFAULT '0',
  `phaseUseFlags` tinyint unsigned NOT NULL DEFAULT '0',
  `PhaseId` int DEFAULT '0',
  `PhaseGroup` int DEFAULT '0',
  `terrainSwapMap` int NOT NULL DEFAULT '-1',
  `modelid` int unsigned NOT NULL DEFAULT '0',
  `equipment_id` tinyint NOT NULL DEFAULT '0',
  `position_x` float NOT NULL DEFAULT '0',
  `position_y` float NOT NULL DEFAULT '0',
  `position_z` float NOT NULL DEFAULT '0',
  `orientation` float NOT NULL DEFAULT '0',
  `spawntimesecs` int unsigned NOT NULL DEFAULT '120',
  `wander_distance` float NOT NULL DEFAULT '0',
  `currentwaypoint` int unsigned NOT NULL DEFAULT '0',
  `curHealthPct` int unsigned NOT NULL DEFAULT '100',
  `MovementType` tinyint unsigned NOT NULL DEFAULT '0',
  `npcflag` bigint unsigned DEFAULT NULL,
  `unit_flags` int unsigned DEFAULT NULL,
  `unit_flags2` int unsigned DEFAULT NULL,
  `unit_flags3` int unsigned DEFAULT NULL,
  `ScriptName` varchar(64) NOT NULL DEFAULT '',
  `StringId` varchar(64) DEFAULT NULL,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`),
  KEY `idx_map` (`map`),
  KEY `idx_id` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='Creature spawns (world placement)';

CREATE TABLE IF NOT EXISTS `creature_addon` (
  `guid` bigint unsigned NOT NULL DEFAULT '0',
  `PathId` int unsigned NOT NULL DEFAULT '0',
  `mount` int unsigned NOT NULL DEFAULT '0',
  `MountCreatureID` int unsigned NOT NULL DEFAULT '0',
  `StandState` tinyint unsigned NOT NULL DEFAULT '0',
  `AnimTier` tinyint unsigned NOT NULL DEFAULT '0',
  `VisFlags` tinyint unsigned NOT NULL DEFAULT '0',
  `SheathState` tinyint unsigned NOT NULL DEFAULT '1',
  `PvPFlags` tinyint unsigned NOT NULL DEFAULT '0',
  `emote` int unsigned NOT NULL DEFAULT '0',
  `aiAnimKit` smallint unsigned NOT NULL DEFAULT '0',
  `movementAnimKit` smallint unsigned NOT NULL DEFAULT '0',
  `meleeAnimKit` smallint unsigned NOT NULL DEFAULT '0',
  `visibilityDistanceType` tinyint unsigned NOT NULL DEFAULT '0',
  `auras` mediumtext,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `creature_template_addon` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `PathId` int unsigned NOT NULL DEFAULT '0',
  `mount` int unsigned NOT NULL DEFAULT '0',
  `MountCreatureID` int unsigned NOT NULL DEFAULT '0',
  `StandState` tinyint unsigned NOT NULL DEFAULT '0',
  `AnimTier` tinyint unsigned NOT NULL DEFAULT '0',
  `VisFlags` tinyint unsigned NOT NULL DEFAULT '0',
  `SheathState` tinyint unsigned NOT NULL DEFAULT '1',
  `PvPFlags` tinyint unsigned NOT NULL DEFAULT '0',
  `emote` int unsigned NOT NULL DEFAULT '0',
  `aiAnimKit` smallint unsigned NOT NULL DEFAULT '0',
  `movementAnimKit` smallint unsigned NOT NULL DEFAULT '0',
  `meleeAnimKit` smallint unsigned NOT NULL DEFAULT '0',
  `visibilityDistanceType` tinyint unsigned NOT NULL DEFAULT '0',
  `auras` mediumtext,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `creature_template` (`entry`, `name`, `subname`) VALUES
  (6, 'Kobold Vermin', 'Monster'),
  (2575, 'Harlan Bagley', 'Baker'),
  (35176, 'Stormwind Mage', 'Portal Trainer')
ON DUPLICATE KEY UPDATE
  `name` = VALUES(`name`),
  `subname` = VALUES(`subname`);

-- === 25_world_creature_classlevelstats.sql ===
-- Per-level creature stats (`creature_classlevelstats`) + template level range.
-- JDBC-safe: seed uses INSERT…SELECT + recursive CTE (MySQL 8+), no procedures.

USE `firelands_world`;

CREATE TABLE IF NOT EXISTS `creature_classlevelstats` (
  `level` tinyint unsigned NOT NULL,
  `class` tinyint unsigned NOT NULL COMMENT 'Creature UnitClass (matches creature_template.unit_class; 0 = warrior fallback in core)',
  `basemana` int unsigned NOT NULL DEFAULT '0',
  `attackpower` smallint NOT NULL DEFAULT '0',
  `rangedattackpower` smallint NOT NULL DEFAULT '0',
  `basehealth` int unsigned NOT NULL DEFAULT '100',
  `comment` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`level`,`class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT IGNORE INTO `creature_classlevelstats`
  (`level`, `class`, `basemana`, `attackpower`, `rangedattackpower`, `basehealth`, `comment`)
WITH RECURSIVE `lvl` (`n`) AS (
  SELECT 1 AS `n`
  UNION ALL
  SELECT `n` + 1 FROM `lvl` WHERE `n` < 85
),
`cls` (`c`) AS (
  SELECT 1 AS `c`
  UNION ALL
  SELECT `c` + 1 FROM `cls` WHERE `c` < 11
)
SELECT
  `lvl`.`n`,
  `cls`.`c`,
  CASE WHEN `cls`.`c` IN (1, 4) THEN 0 ELSE LEAST(60000, 80 + `lvl`.`n` * 45) END,
  CAST(5 + `lvl`.`n` * 3 AS SIGNED),
  CAST(
    CASE WHEN `cls`.`c` IN (3, 8, 9, 11) THEN 5 + `lvl`.`n` * 3
         ELSE (5 + `lvl`.`n` * 3) DIV 2 END AS SIGNED),
  LEAST(4000000, 45 + `lvl`.`n` * 18 + `cls`.`c` * 4),
  'Firelands seed'
FROM `lvl` CROSS JOIN `cls`;

SET @exist_minlevel :=
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
   WHERE TABLE_SCHEMA = DATABASE()
     AND TABLE_NAME = 'creature_template'
     AND COLUMN_NAME = 'minlevel');

SET @sql_minmax := IF(@exist_minlevel = 0,
  'ALTER TABLE `creature_template`
     ADD COLUMN `minlevel` tinyint unsigned NOT NULL DEFAULT ''1'' AFTER `VerifiedBuild`,
     ADD COLUMN `maxlevel` tinyint unsigned NOT NULL DEFAULT ''1'' AFTER `minlevel`',
  'SELECT 1');

PREPARE stmt_minmax FROM @sql_minmax;
EXECUTE stmt_minmax;
DEALLOCATE PREPARE stmt_minmax;

UPDATE `creature_template`
SET `minlevel` = 3, `maxlevel` = 5, `unit_class` = 1
WHERE `entry` = 6;

INSERT IGNORE INTO `creature` (
  `guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `phaseUseFlags`,
  `PhaseId`, `PhaseGroup`, `terrainSwapMap`, `modelid`, `equipment_id`,
  `position_x`, `position_y`, `position_z`, `orientation`,
  `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curHealthPct`,
  `MovementType`, `npcflag`, `unit_flags`, `unit_flags2`, `unit_flags3`,
  `ScriptName`, `StringId`, `VerifiedBuild`
) VALUES (
  9000001, 6, 0, 9, 9, '0', 0,
  0, 0, -1, 2576, 0,
  -8949.0, -132.0, 83.5, 5.2,
  120, 0, 0, 100,
  0, NULL, NULL, NULL, NULL,
  '', NULL, 0
);

-- === 26_world_instance_tables.sql ===
-- Instance metadata from reference implementation `data/sql/base/db_world/`
-- Tables: instance_template, instance_encounters, instance_spawn_groups.
-- JDBC-safe (semicolon-separated statements).

USE `firelands_world`;

CREATE TABLE IF NOT EXISTS `instance_template` (
  `map` smallint unsigned NOT NULL,
  `parent` smallint unsigned NOT NULL,
  `script` varchar(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `allowMount` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`map`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `instance_encounters` (
  `entry` int unsigned NOT NULL COMMENT 'Unique entry from DungeonEncounter.dbc',
  `creditType` tinyint unsigned NOT NULL DEFAULT '0',
  `creditEntry` int unsigned NOT NULL DEFAULT '0',
  `lastEncounterDungeon` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'If not 0, LfgDungeon.dbc entry for the instance it is last encounter in',
  `comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `instance_spawn_groups` (
  `instanceMapId` smallint unsigned NOT NULL,
  `bossStateId` tinyint unsigned NOT NULL,
  `bossStates` tinyint unsigned NOT NULL,
  `spawnGroupId` int unsigned NOT NULL,
  `flags` tinyint unsigned NOT NULL,
  PRIMARY KEY (`instanceMapId`,`bossStateId`,`spawnGroupId`,`bossStates`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELETE FROM `instance_spawn_groups`;
DELETE FROM `instance_encounters`;
DELETE FROM `instance_template`;

INSERT INTO `instance_template` VALUES (30,0,'',1),(33,0,'instance_shadowfang_keep',0),(34,0,'instance_the_stockade',0),(36,0,'instance_deadmines',0),(43,1,'instance_wailing_caverns',0),(47,1,'instance_razorfen_kraul',0),(48,1,'instance_blackfathom_deeps',0),(70,0,'instance_uldaman',0),(90,0,'instance_gnomeregan',0),(109,0,'instance_sunken_temple',0),(129,1,'instance_razorfen_downs',0),(169,0,'',0),(189,0,'instance_scarlet_monastery',0),(209,1,'instance_zulfarrak',1),(229,0,'instance_blackrock_spire',0),(230,0,'instance_blackrock_depths',0),(249,1,'instance_onyxias_lair',0),(269,1,'instance_the_black_morass',1),(289,0,'instance_scholomance',0),(329,0,'instance_stratholme',0),(349,1,'instance_maraudon',0),(389,1,'instance_ragefire_chasm',0),(409,230,'instance_molten_core',0),(429,1,'instance_dire_maul',0),(469,229,'instance_blackwing_lair',0),(489,0,'',1),(509,1,'instance_ruins_of_ahnqiraj',1),(529,0,'',1),(531,1,'instance_temple_of_ahnqiraj',1),(532,0,'instance_karazhan',0),(533,571,'instance_naxxramas',0),(534,1,'instance_hyjal',1),(540,530,'instance_shattered_halls',0),(542,530,'instance_blood_furnace',0),(543,530,'instance_ramparts',0),(544,530,'instance_magtheridons_lair',0),(545,530,'instance_steam_vault',0),(546,530,'instance_the_underbog',0),(547,530,'instance_the_slave_pens',0),(548,530,'instance_serpent_shrine',0),(550,530,'instance_the_eye',0),(552,530,'instance_arcatraz',0),(553,530,'instance_the_botanica',0),(554,530,'instance_mechanar',0),(555,530,'instance_shadow_labyrinth',0),(556,530,'instance_sethekk_halls',0),(557,530,'instance_mana_tombs',0),(558,530,'instance_auchenai_crypts',0),(559,0,'',1),(560,1,'instance_old_hillsbrad',1),(562,0,'',1),(564,530,'instance_black_temple',1),(565,530,'instance_gruuls_lair',0),(566,0,'',1),(568,0,'instance_zulaman',1),(572,0,'',1),(574,571,'instance_utgarde_keep',1),(575,571,'instance_utgarde_pinnacle',0),(576,571,'instance_nexus',0),(578,571,'instance_oculus',1),(580,530,'instance_sunwell_plateau',1),(585,530,'instance_magisters_terrace',0),(595,1,'instance_culling_of_stratholme',1),(599,571,'instance_halls_of_stone',0),(600,571,'instance_drak_tharon_keep',1),(601,571,'instance_azjol_nerub',0),(602,571,'instance_halls_of_lightning',0),(603,571,'instance_ulduar',1),(604,571,'instance_gundrak',0),(607,0,'',1),(608,571,'instance_violet_hold',0),(615,571,'instance_obsidian_sanctum',1),(616,571,'instance_eye_of_eternity',0),(619,571,'instance_ahnkahet',0),(624,571,'instance_vault_of_archavon',0),(628,0,'',1),(631,571,'instance_icecrown_citadel',1),(632,571,'instance_forge_of_souls',0),(643,0,'instance_throne_of_the_tides',0),(644,1,'instance_halls_of_origination',0),(645,0,'instance_blackrock_caverns',0),(649,571,'instance_trial_of_the_crusader',0),(650,571,'instance_trial_of_the_champion',0),(657,1,'instance_vortex_pinnacle',0),(658,571,'instance_pit_of_saron',1),(668,571,'instance_halls_of_reflection',1),(669,0,'instance_blackwing_descent',0),(670,0,'instance_grim_batol',0),(671,0,'instance_bastion_of_twilight',0),(720,1,'instance_firelands',1),(724,571,'instance_ruby_sanctum',1),(725,0,'instance_stonecore',0),(734,1,'',0),(754,1,'instance_throne_of_the_four_winds',0),(755,1,'instance_lost_city_of_the_tolvir',1),(757,732,'instance_baradin_hold',0),(859,0,'instance_zulgurub',1),(938,1,'instance_end_time',1),(939,1,'instance_well_of_eternity',1),(940,1,'instance_hour_of_twilight',1),(967,1,'instance_dragon_soul',1);

INSERT INTO `instance_encounters` VALUES (201,0,18371,0,'Shirrak the Dead Watcher'),(202,0,18373,149,'Exarch Maladaar'),(203,0,18341,0,'Pandemonius'),(204,0,18343,0,'Tavarok'),(205,0,18344,148,'Nexus-Prince Shaffar'),(206,0,18472,0,'Darkweaver Syth'),(207,0,18473,150,'Talon King Ikiss'),(208,0,18731,0,'Ambassador Hellmaw'),(209,0,18667,0,'Blackheart the Inciter'),(210,0,18732,0,'Grandmaster Vorpil'),(211,0,18708,151,'Murmur'),(212,0,29309,0,'Elder Nadox'),(213,0,29308,0,'Prince Taldaram'),(214,0,29310,0,'Jedoga Shadowseeker'),(215,0,29311,218,'Herald Volazj'),(216,0,28684,0,'Krik\'thir the Gatewatcher'),(217,0,28921,0,'Hadronox'),(218,0,29120,204,'Anub\'arak'),(219,0,4887,0,'Ghamoo-ra'),(220,0,4831,0,'Lady Sarevess'),(221,0,6243,0,'Gelihast'),(222,0,12902,0,'Lorgus Jett'),(224,0,4830,0,'Old Serra\'kis'),(225,0,4832,0,'Twilight Lord Kelris'),(226,0,4829,10,'Aku\'mai'),(227,0,9018,30,'High Interrogator Gerstahn'),(228,0,9025,0,'Lord Roccor'),(229,0,9319,0,'Houndmaster Grebmar'),(230,0,10096,0,'Ring of Law'),(231,0,9024,0,'Pyromancer Loregrain'),(232,0,9017,0,'Lord Incendius'),(233,0,9041,0,'Warder Stilgiss'),(234,0,9056,0,'Fineous Darkvire'),(235,0,9016,0,'Bael\'Gar'),(236,0,9033,0,'General Angerforge'),(237,0,8983,0,'Golem Lord Argelmach'),(238,0,9537,0,'Hurley Blackbreath'),(239,0,9502,0,'Phalanx'),(240,0,9543,0,'Ribbly Screwspigot'),(241,0,9499,0,'Plugger Spazzring'),(242,0,9156,0,'Ambassador Flamelash'),(243,0,9035,0,'The Seven'),(244,0,9938,0,'Magmus'),(245,0,9019,276,'Emperor Dagran Thaurissan'),(246,0,18371,0,'Shirrak the Dead Watcher'),(247,0,18373,178,'Exarch Maladaar'),(248,0,18341,0,'Pandemonius'),(249,0,18343,0,'Tavarok'),(250,0,22930,0,'Yor'),(251,0,18344,179,'Nexus-Prince Shaffar'),(252,0,18472,0,'Darkweaver Syth'),(253,0,23035,0,'Anzu'),(254,0,18473,180,'Talon King Ikiss'),(255,0,18731,0,'Ambassador Hellmaw'),(256,0,18667,0,'Blackheart the Inciter'),(257,0,18732,0,'Grandmaster Vorpil'),(258,0,18708,181,'Murmur'),(259,0,29309,0,'Elder Nadox'),(260,0,29308,0,'Prince Taldaram'),(261,0,29310,0,'Jedoga Shadowseeker'),(262,0,30258,0,'Amanitar'),(263,0,29311,219,'Herald Volazj'),(264,0,28684,0,'Krik\'thir the Gatewatcher'),(265,0,28921,0,'Hadronox'),(266,0,29120,241,'Anub\'arak'),(267,0,9196,0,'Highlord Omokk'),(268,0,9236,0,'Shadow Hunter Vosh\'gajin'),(269,0,9237,0,'War Master Voone'),(270,0,10596,0,'Mother Smolderweb'),(271,0,10584,0,'Urok Doomhowl'),(272,0,9736,0,'Quartermaster Zigris'),(273,0,10268,0,'Gizrul the Slavener'),(274,0,10220,0,'Halycon'),(275,0,9568,32,'Overlord Wyrmthalak'),(276,0,9816,0,'Pyroguard Emberseer'),(277,0,10264,0,'Solakar Flamewreath'),(278,0,10429,0,'Warchief Rend Blackhand'),(279,0,10430,0,'The Beast'),(280,0,10363,0,'General Drakkisath'),(281,0,18096,170,'Epoch Hunter'),(282,0,18096,183,'Epoch Hunter'),(283,0,17862,0,'Captain Skarloc'),(284,0,17862,0,'Captain Skarloc'),(285,0,17848,0,'Lieutenant Drake'),(286,0,17848,0,'Lieutenant Drake'),(287,0,17879,0,'Chrono Lord Deja'),(288,0,17879,0,'Chrono Lord Deja'),(289,0,17880,0,'Temporus'),(290,0,17880,0,'Temporus'),(291,0,17881,171,'Aeonus'),(292,0,17881,182,'Aeonus'),(293,0,26529,0,'Meathook'),(294,0,26530,0,'Salram the Fleshcrafter'),(295,0,26532,0,'Chrono-Lord Epoch'),(296,1,58630,0,'Mal\'ganis'),(297,0,26529,0,'Meathook'),(298,0,26530,0,'Salram the Fleshcrafter'),(299,0,26532,0,'Chrono-Lord Epoch'),(300,1,58630,0,'Mal\'ganis'),(301,0,17941,0,'Mennu the Betrayer'),(302,0,17991,0,'Rokmar the Crackler'),(303,0,17942,140,'Quagmirran'),(304,0,17941,0,'Mennu the Betrayer'),(305,0,17991,0,'Rokmar the Crackler'),(306,0,17942,184,'Quagmirran'),(314,0,17797,0,'Hydromancer Thespia'),(315,0,17797,0,'Hydromancer Thespia'),(316,0,17796,0,'Mekgineer Steamrigger'),(317,0,17796,0,'Mekgineer Steamrigger'),(318,0,17798,147,'Warlord Kalithresh'),(319,0,17798,185,'Warlord Kalithresh'),(320,0,17770,0,'Hungarfen'),(321,0,17770,0,'Hungarfen'),(322,0,18105,0,'Ghaz\'an'),(323,0,18105,0,'Ghaz\'an'),(329,0,17826,0,'Swamplord Musel\'ek'),(330,0,17826,0,'Swamplord Musel\'ek'),(331,0,17882,146,'The Black Stalker'),(332,0,17882,186,'The Black Stalker'),(334,1,68572,0,'Grand Champions'),(336,1,68572,0,'Grand Champions'),(338,1,68574,0,'Argent Champion'),(339,1,68574,0,'Argent Champion'),(340,1,68663,245,'The Black Knight'),(341,1,68663,249,'The Black Knight'),(343,0,11490,0,'Zevrim Thornhoof'),(344,0,13280,0,'Hydrospawn'),(345,0,14327,0,'Lethtendris'),(346,0,11492,34,'Alzzin the Wildshaper'),(347,0,11488,0,'Illyanna Ravenoak'),(348,0,11487,0,'Magister Kalendris'),(349,0,11496,0,'Immol\'thar'),(350,0,11489,0,'Tendris Warpwood'),(361,0,11486,36,'Prince Tortheldrin'),(362,0,14326,0,'Guard Mol\'dar'),(363,0,14322,0,'Stomper Kreeg'),(364,0,14321,0,'Guard Fengus'),(365,0,14323,0,'Guard Slip\'kik'),(366,0,14325,0,'Captain Kromcrush'),(367,0,14324,0,'Cho\'Rush the Observer'),(368,0,11501,38,'King Gordok'),(369,0,26630,0,'Trollgore'),(370,0,26630,0,'Trollgore'),(371,0,26631,0,'Novos the Summoner'),(372,0,26631,0,'Novos the Summoner'),(373,0,27483,0,'King Dred'),(374,0,27483,0,'King Dred'),(375,1,61863,214,'The Prophet Tharon\'ja'),(376,1,61863,215,'The Prophet Tharon\'ja'),(378,0,7079,0,'Viscous Fallout'),(379,0,7361,0,'Grubbis'),(380,0,6235,0,'Electrocutioner 6000'),(381,0,6229,0,'Crowd Pummeler 9-60'),(382,0,7800,14,'Mekgineer Thermaplugg'),(383,0,29304,0,'Slad\'ran'),(384,0,29304,0,'Slad\'ran'),(385,0,29573,0,'Drakkari Colossus'),(386,0,29573,0,'Drakkari Colossus'),(387,0,29305,0,'Moorabi'),(388,0,29305,0,'Moorabi'),(389,0,29932,0,'Eck the Ferocious'),(390,0,29306,216,'Gal\'darah'),(391,0,29306,217,'Gal\'darah'),(392,0,17306,0,'Watchkeeper Gargolmar'),(393,0,17306,0,'Watchkeeper Gargolmar'),(394,0,17308,0,'Omor the Unscarred'),(395,0,17308,0,'Omor the Unscarred'),(396,0,17537,136,'Vazruden the Herald'),(397,0,17537,188,'Vazruden the Herald'),(401,0,17381,0,'The Maker'),(402,0,17381,0,'The Maker'),(403,0,17380,0,'Broggok'),(404,0,17380,0,'Broggok'),(405,0,17377,137,'Keli\'dan the Breaker'),(406,0,17377,187,'Keli\'dan the Breaker'),(407,0,16807,0,'Grand Warlock Nethekurse'),(408,0,16807,0,'Grand Warlock Nethekurse'),(409,0,20923,0,'Blood Guard Porung'),(410,0,16809,0,'Warbringer O\'mrogg'),(411,0,16809,0,'Warbringer O\'mrogg'),(412,0,16808,138,'Warchief Kargath Bladefist'),(413,0,16808,189,'Warchief Kargath Bladefist'),(414,0,24723,0,'Selin Fireheart'),(415,0,24723,0,'Selin Fireheart'),(416,0,24744,0,'Vexallus'),(417,0,24744,0,'Vexallus'),(418,0,24560,0,'Priestess Delrissa'),(419,0,24560,0,'Priestess Delrissa'),(420,0,24664,198,'Kael\'thas Sunstrider'),(421,0,24664,201,'Kael\'thas Sunstrider'),(422,0,13282,0,'Noxxion'),(423,0,12258,26,'Razorlash'),(424,0,12236,272,'Lord Vyletongue'),(425,0,12225,0,'Celebras the Cursed'),(426,0,12203,0,'Landslide'),(427,0,13601,0,'Tinkerer Gizlock'),(428,0,13596,0,'Rotgrip'),(429,0,12201,273,'Princess Theradras'),(430,0,11517,0,'Oggleflint'),(431,0,11520,4,'Taragaman the Hungerer'),(432,0,11518,0,'Jergosh the Invoker'),(433,0,11519,0,'Bazzalan'),(434,0,7355,0,'Tuten\'kash'),(435,0,7357,0,'Mordresh Fire Eye'),(436,0,8567,0,'Glutton'),(437,0,7358,20,'Amnennar the Coldbringer'),(438,0,6168,0,'Roogug'),(439,0,4424,0,'Aggem Thorncurse'),(440,0,4428,0,'Death Speaker Jargba'),(441,0,4420,0,'Overlord Ramtusk'),(443,0,4421,16,'Charlga Razorflank'),(444,0,3983,0,'Interrogator Vishas'),(445,0,4543,18,'Bloodmage Thalnos'),(446,0,3974,0,'Houndmaster Loksey'),(447,0,6487,165,'Arcanist Doan'),(448,0,3975,163,'Herod'),(449,0,4542,0,'High Inquisitor Fairbanks'),(450,0,3977,164,'High Inquisitor Whitemane'),(451,0,10506,0,'Kirtonos the Herald'),(452,0,10503,0,'Jandice Barov'),(453,0,11622,0,'Rattlegore'),(454,0,10433,0,'Marduk Blackpool'),(455,0,10432,0,'Vectus'),(456,0,10508,0,'Ras Frostwhisper'),(457,0,10505,0,'Instructor Malicia'),(458,0,11261,0,'Doctor Theolen Krastinov'),(459,0,10901,0,'Lorekeeper Polkelt'),(460,0,10507,0,'The Ravenian'),(461,0,10504,0,'Lord Alexei Barov'),(462,0,10502,0,'Lady Illucia Barov'),(463,0,1853,2,'Darkmaster Gandling'),(472,0,10516,0,'The Unforgiven'),(473,0,10558,0,'Hearthsinger Forresten'),(474,0,10808,0,'Timmy the Cruel'),(475,0,10997,0,'Willey Hopebreaker'),(476,0,11032,0,'Commander Malor'),(477,0,10811,0,'Instructor Galford'),(478,0,10813,40,'Balnazzar'),(479,0,10436,0,'Baroness Anastari'),(480,0,10437,0,'Nerub\'enkan'),(481,0,10438,0,'Maleki the Pallid'),(482,0,10435,0,'Magistrate Barthilas'),(483,0,10439,0,'Ramstein the Gorger'),(484,0,10440,274,'Lord Aurius Rivendare'),(486,0,5721,0,'Dreamscythe'),(487,0,5720,0,'Weaver'),(488,0,5710,0,'Jammal\'an the Prophet'),(490,0,5719,0,'Morphaz'),(491,0,5722,0,'Hazzas'),(492,0,8443,0,'Avatar of Hakkar'),(493,0,5709,28,'Shade of Eranikus'),(494,0,20870,0,'Zereketh the Unbound'),(495,0,20870,0,'Zereketh the Unbound'),(496,0,20885,0,'Dalliah the Doomsayer'),(497,0,20885,0,'Dalliah the Doomsayer'),(498,0,20886,0,'Wrath-Scryer Soccothrates'),(499,0,20886,0,'Wrath-Scryer Soccothrates'),(500,0,20912,174,'Harbinger Skyriss'),(501,0,20912,190,'Harbinger Skyriss'),(502,0,17976,0,'Commander Sarannis'),(504,0,17976,0,'Commander Sarannis'),(505,0,17975,0,'High Botanist Freywinn'),(506,0,17975,0,'High Botanist Freywinn'),(507,0,17978,0,'Thorngrin the Tender'),(508,0,17978,0,'Thorngrin the Tender'),(509,0,17980,0,'Laj'),(510,0,17980,0,'Laj'),(511,0,17977,173,'Warp Splinter'),(512,0,17977,191,'Warp Splinter'),(513,0,19219,0,'Mechano-Lord Capacitus'),(514,0,19219,0,'Mechano-Lord Capacitus'),(515,0,19221,0,'Nethermancer Sepethrea'),(516,0,19221,0,'Nethermancer Sepethrea'),(517,0,19220,172,'Pathaleon the Calculator'),(518,0,19220,192,'Pathaleon the Calculator'),(519,0,26796,0,'Frozen Commander'),(520,0,26731,0,'Grand Magus Telestra'),(521,0,26731,0,'Grand Magus Telestra'),(522,0,26763,0,'Anomalus'),(523,0,26763,0,'Anomalus'),(524,0,26794,0,'Ormorok the Tree-Shaper'),(525,0,26794,0,'Ormrok the Tree-Shaper'),(526,0,26723,225,'Keristrasza'),(527,0,26723,226,'Keristrasza'),(528,0,27654,0,'Drakos the Interrogator'),(529,0,27654,0,'Drakos the Interrogator'),(530,0,27447,0,'Varos Cloudstrider'),(531,0,27447,0,'Varos Cloudstrider'),(532,0,27655,0,'Mage-Lord Urom'),(533,0,27655,0,'Mage-Lord Urom'),(534,0,27656,206,'Ley-Guardian Eregos'),(535,0,27656,211,'Ley-Guardian Eregos'),(541,0,29315,0,'First Prisoner'),(542,0,29315,0,'First Prisoner'),(543,0,29316,0,'Second Prisoner'),(544,0,29316,0,'Second Prisoner'),(545,0,31134,220,'Cyanigosa'),(546,0,31134,221,'Cyanigosa'),(547,0,6910,0,'Revelosh'),(548,0,6906,0,'The Lost Dwarves'),(549,0,7228,0,'Ironaya'),(551,0,7206,0,'Ancient Stone Keeper'),(552,0,7291,0,'Galgann Firehammer'),(553,0,4854,0,'Grimlok'),(554,0,2748,22,'Archaedas'),(555,0,28586,0,'General Bjarngrim'),(556,0,28586,0,'General Bjarngrim'),(557,0,28587,0,'Volkhan'),(558,0,28587,0,'Volkhan'),(559,0,28546,0,'Ionar'),(560,0,28546,0,'Ionar'),(561,0,28923,207,'Loken'),(562,0,28923,212,'Loken'),(563,0,27977,0,'Krystallus'),(564,0,27977,0,'Krystallus'),(565,0,27975,0,'Maiden of Grief'),(566,0,27975,0,'Maiden of Grief'),(567,1,59046,0,'Tribunal of Ages'),(568,1,59046,0,'Tribunal of Ages'),(569,0,27978,208,'Sjonnir the Ironshaper'),(570,0,27978,213,'Sjonnir the Ironshaper'),(571,0,23953,0,'Prince Keleseth'),(572,0,23953,0,'Prince Keleseth'),(573,0,24201,0,'Skarvold & Dalronn'),(574,0,24201,0,'Skarvold & Dalronn'),(575,0,23980,202,'Ingvar the Plunderer'),(576,0,23980,242,'Ingvar the Plunderer'),(577,0,26668,0,'Svala Sorrowgrave'),(578,0,26668,0,'Svala Sorrowgrave'),(579,0,26687,0,'Gortok Palehoof'),(580,0,26687,0,'Gortok Palehoof'),(581,0,26693,0,'Skadi the Ruthless'),(582,0,26693,0,'Skadi the Ruthless'),(583,0,26861,203,'King Ymiron'),(584,0,26861,205,'King Ymiron'),(585,0,3671,0,'Lady Anacondra'),(586,0,3669,0,'Lord Cobrahn'),(587,0,3653,0,'Kresh'),(588,0,3670,0,'Lord Pythas'),(589,0,3674,0,'Skum'),(590,0,3673,0,'Lord Serpentis'),(591,0,5775,0,'Verdan the Everliving'),(592,0,3654,1,'Mutanus the Devourer'),(593,0,7795,0,'Hydromancer Velratha'),(594,0,7273,0,'Ghaz\'rilla'),(595,0,8127,0,'Antu\'sul'),(596,0,7272,0,'Theka the Martyr'),(597,0,7271,0,'Witch Doctor Zum\'rah'),(598,0,7796,0,'Nekrum Gutchewer'),(599,0,7275,0,'Shadowpriest Sezz\'ziz'),(600,0,7267,24,'Chief Ukorz Sandscalp'),(601,0,22887,0,'High Warlord Naj\'entus'),(602,0,22898,0,'Supremus'),(603,0,22841,0,'Shade of Akama'),(604,0,22871,0,'Teron Gorefiend'),(605,0,22948,0,'Gurtogg Bloodboil'),(606,0,23420,0,'Reliquary of Souls'),(607,0,22947,0,'Mother Shahraz'),(608,0,23426,0,'The Illidari Council'),(609,0,22917,196,'Illidan Stormrage'),(610,0,12435,0,'Razorgore the Untamed'),(611,0,13020,0,'Vaelastrasz the Corrupt'),(612,0,12017,0,'Broodlord Lashlayer'),(613,0,11983,0,'Firemaw'),(614,0,14601,0,'Ebonroc'),(615,0,11981,0,'Flamegor'),(616,0,14020,0,'Chromaggus'),(617,0,11583,50,'Nefarian'),(618,0,17767,0,'Rage Winterchill'),(619,0,17808,0,'Anetheron'),(620,0,17888,0,'Kaz\'rogal'),(621,0,17842,0,'Azgalor'),(622,0,17968,195,'Archimonde'),(623,0,21216,0,'Hydross the Unstable'),(624,0,21217,0,'The Lurker Below'),(625,0,21215,0,'Leotheras the Blind'),(626,0,21214,0,'Fathom-Lord Karathress'),(627,0,21213,0,'Morogrim Tidewalker'),(628,0,21212,194,'Lady Vashj'),(649,0,18831,0,'High King Maulgar'),(650,0,19044,177,'Gruul the Dragonkiller'),(651,0,17257,176,'Magtheridon'),(652,0,15550,0,'Attumen the Huntsman'),(653,0,15687,0,'Moroes'),(654,0,16457,0,'Maiden of the Virtue'),(655,0,16812,0,'Opera Event'),(656,0,15691,0,'The Curator'),(657,0,15688,0,'Terestian Illhoof'),(658,0,16524,0,'Shade of Aran'),(659,0,15689,0,'Netherspite'),(660,0,22520,0,'Chess Event'),(661,0,15690,175,'Prince Malchezaar'),(662,0,17225,0,'Nightbane'),(663,0,12118,0,'Lucifron'),(664,0,11982,0,'Magmadar'),(665,0,12259,0,'Gehennas'),(666,0,12057,0,'Garr'),(667,0,12264,0,'Shazzrah'),(668,0,12056,0,'Baron Geddon'),(669,0,12098,0,'Sulfuron Harbinger'),(670,0,11988,0,'Golemagg the Incinerator'),(671,0,12018,0,'Majordomo Executus'),(672,0,11502,48,'Ragnaros'),(709,0,15263,0,'The Prophet Skeram'),(710,0,15544,0,'Silithid Royalty'),(711,0,15516,0,'Battleguard Sartura'),(712,0,15510,0,'Fankriss the Unyielding'),(713,0,15299,0,'Viscidus'),(714,0,15509,0,'Princess Huhuran'),(715,0,15275,0,'Twin Emperors'),(716,0,15517,0,'Ouro'),(717,0,15727,161,'C\'thun'),(718,0,15348,0,'Kurinnaxx'),(719,0,15341,0,'General Rajaxx'),(720,0,15340,0,'Moam'),(721,0,15370,0,'Buru the Gorger'),(722,0,15369,0,'Ayamiss the Hunter'),(723,0,15339,160,'Ossirian the Unscarred'),(724,0,24892,0,'Kalecgos'),(725,0,24882,0,'Brutallus'),(726,0,25038,0,'Felmyst'),(727,0,25165,0,'Eredar Twins'),(728,0,25840,0,'M\'uru'),(729,0,25315,199,'Kil\'jaeden'),(730,0,19514,0,'Al\'ar'),(731,0,19516,0,'Void Reaver'),(732,0,18805,0,'High Astromancer Solarian'),(733,0,19622,193,'Kael\'thas Sunstrider'),(784,0,14507,0,'High Priest Venoxis'),(785,0,14517,0,'High Priestess Jeklik'),(786,0,14510,0,'High Priestess Mar\'li'),(787,0,11382,0,'Bloodlord Mandokir'),(788,0,15083,0,'Edge of Madness'),(789,0,14509,0,'High Priest Thekal'),(790,0,15114,0,'Gahz\'ranka'),(791,0,14515,0,'High Priestess Arlokk'),(792,0,11380,0,'Jin\'do the Hexxer'),(793,0,14834,0,'Hakkar'),(829,0,36497,0,'Bronjahm'),(830,0,36497,0,'Bronjahm '),(831,0,36502,251,'Devourer of Souls'),(832,0,36502,252,'Devourer of Souls'),(833,0,36494,0,'Forgemaster Garfrost'),(834,0,36494,0,'Forgemaster Garfrost'),(835,0,36476,0,'Krick'),(836,0,36476,0,'Krick'),(837,0,36658,253,'Scourgelord Tyrannus'),(838,0,36658,254,'Scourgelord Tyrannus'),(839,0,38113,0,'Marwyn'),(840,0,38113,0,'Marwyn'),(841,0,38112,0,'Falric'),(842,0,38112,0,'Falric'),(843,1,72830,255,'Escaped from Arthas'),(844,1,72830,256,'Escaped from Arthas'),(883,0,4422,0,'Agathelos the Raging'),(1022,0,41442,0,'Atramedes'),(1023,0,43296,0,'Chimaeron'),(1024,0,41570,0,'Magmaw'),(1025,0,41378,0,'Maloriak'),(1026,0,41376,313,'Nefarian\'s End'),(1027,0,42180,0,'Omnotron Defense System'),(1028,0,43735,0,'Ascendant Council'),(1029,0,43324,315,'Cho\'gall'),(1030,0,44600,0,'Halfus Wyrmbreaker'),(1032,0,45992,0,'Theralion and Valiona'),(1033,0,47120,0,'Argaloth'),(1034,0,46753,317,'Al\'Akir'),(1035,1,88835,0,'Conclave of Wind'),(1036,0,39705,303,'Ascendant Lord Obsidius'),(1037,0,39700,0,'Beauty'),(1038,0,39679,0,'Corla, Herald of Twilight'),(1039,0,39698,0,'Karsh Steelbender'),(1040,0,39665,0,'Rom\'ogg Bonecrusher'),(1041,0,43873,0,'Altairus'),(1042,0,43875,311,'Asaad'),(1043,0,43878,0,'Grand Vizier Ertan'),(1044,0,40765,0,'Commander Ulthok'),(1045,0,40586,0,'Lady Naz\'jar'),(1046,0,40788,0,'Mindbender Ghur\'sha'),(1047,0,44566,302,'Ozumat'),(1048,0,40319,0,'Drahga Shadowburner'),(1049,0,40484,304,'Erudax'),(1050,0,40177,0,'Forgemaster Throngus'),(1051,0,39625,0,'General Umbriss'),(1052,0,44577,0,'General Husam'),(1053,0,43612,0,'High Prophet Barim'),(1054,0,43614,0,'Lockmaw'),(1055,0,44819,312,'Siamat'),(1056,0,43438,0,'Corborus'),(1057,0,42333,307,'High Priestess Azil'),(1058,0,42188,0,'Ozruk'),(1059,0,43214,0,'Slabhide'),(1060,0,47739,6,'\"Captain\" Cookie'),(1062,0,47626,0,'Admiral Ripsnarl'),(1063,0,43778,0,'Foe Reaper 5000'),(1064,0,47162,0,'Glubtok'),(1065,0,47296,0,'Helix Gearbreaker'),(1069,0,46962,0,'Baron Ashbury'),(1070,0,3887,0,'Baron Silverlaine'),(1071,0,4278,0,'Commander Springvale'),(1072,0,46964,8,'Lord Godfrey'),(1073,0,46963,0,'Lord Walden'),(1074,0,39731,0,'Ammunae'),(1075,0,39788,0,'Anraphet'),(1076,0,39428,0,'Earthrager Ptah'),(1077,0,39587,0,'Isiset'),(1078,0,39378,305,'Rajh'),(1079,0,39732,0,'Setesh'),(1080,0,39425,0,'Temple Guardian Anhuur'),(1081,0,49541,326,'Vanessa VanCleef'),(1082,0,45213,0,'Sinestra'),(1083,0,49744,0,'Sinestra'),(1084,0,10184,46,'Onyxia'),(1085,0,34564,246,'Anub\'arak'),(1086,1,68184,0,'Faction Champions'),(1087,0,34780,0,'Lord Jaraxxus'),(1088,0,34797,0,'Northrend Beasts'),(1089,0,34496,0,'Val\'kyr Twins'),(1090,0,28860,238,'Sartharion'),(1091,0,30451,0,'Shadron'),(1092,0,30452,0,'Tenebron'),(1093,0,30449,0,'Vesperon'),(1094,0,28859,237,'Malygos'),(1095,0,37970,0,'Blood Council'),(1096,1,72928,0,'Deathbringer Saurfang'),(1097,0,36626,0,'Festergut'),(1098,1,72706,0,'Valithria Dreamwalker'),(1099,1,72959,0,'Icecrown Gunship Battle'),(1100,0,36855,0,'Lady Deathwhisper'),(1101,0,36612,0,'Lord Marrowgar'),(1102,0,36678,0,'Professor Putricide'),(1103,0,37955,0,'Queen Lana\'thel'),(1104,0,36627,0,'Rotface'),(1105,0,36853,0,'Sindragosa'),(1106,0,36597,0,'The Lich King'),(1107,0,15956,0,'Anub\'Rekhan'),(1108,0,15932,0,'Gluth'),(1109,0,16060,0,'Gothik the Harvester'),(1110,0,15953,0,'Grand Widow Faerlina'),(1111,0,15931,0,'Grobbulus'),(1112,0,15936,0,'Heigan the Unclean'),(1113,0,16061,0,'Instructor Razuvious'),(1114,0,15990,159,'Kel\'Thuzad'),(1115,0,16011,0,'Loatheb'),(1116,0,15952,0,'Maexxna'),(1117,0,15954,0,'Noth the Plaguebringer'),(1118,0,16028,0,'Patchwerk'),(1119,0,15989,0,'Sapphiron'),(1120,0,15928,0,'Thaddius'),(1121,1,59450,0,'The Four Horsemen'),(1126,0,31125,0,'Archavon the Stone Watcher'),(1127,0,33993,0,'Emalon the Storm Watcher'),(1128,0,35013,0,'Koralon the Flame Watcher'),(1129,0,38433,239,'Toravon the Ice Watcher'),(1130,1,65184,243,'Algalon the Observer'),(1131,0,33515,0,'Auriaya'),(1132,0,33113,0,'Flame Leviathan'),(1133,1,65074,0,'Freya'),(1134,0,33271,0,'General Vezax'),(1135,1,64899,0,'Hodir'),(1136,0,33118,0,'Ignis the Furnace Master'),(1137,0,32930,0,'Kologarn'),(1138,0,33432,0,'Mimiron'),(1139,0,33186,0,'Razorscale'),(1140,1,65195,0,'The Assembly of Iron'),(1141,1,64985,0,'Thorim'),(1142,0,33293,0,'XT-002 Deconstructor'),(1143,0,33288,0,'Yogg-Saron'),(1144,0,46264,12,'Hogger'),(1145,0,46254,0,'Lord Overheat'),(1146,0,46383,0,'Randolph Moloch'),(1147,0,39751,0,'Baltharus the Warborn'),(1148,0,39746,0,'General Zarithrian'),(1149,0,39747,0,'Saviana Ragefire'),(1150,0,39863,0,'Halion'),(1164,0,32915,0,'Elder Brightleaf'),(1165,0,32913,0,'Elder Ironbranch'),(1166,0,32914,0,'Elder Stonebark'),(1178,0,52155,0,'High Priest Venoxis'),(1179,0,52151,0,'Bloodlord Mandokir'),(1180,0,52059,0,'High Priestess Kilnara'),(1181,0,52053,0,'Zanzil'),(1182,0,52148,334,'Jin\'do the Godbreaker'),(1185,0,52571,0,'Majordomo Staghelm'),(1188,0,52271,0,'Cache of Madness'),(1189,0,23574,0,'Akil\'zon'),(1190,0,23576,0,'Nalorakk'),(1191,0,23578,0,'Jan\'alai'),(1192,0,23577,0,'Halazzi'),(1193,0,24239,0,'Hex Lord Malacrass'),(1194,0,23863,340,'Daakara'),(1197,0,52498,0,'Beth\'tilac'),(1200,0,53494,0,'Baleroc'),(1203,1,102237,361,'Ragnaros'),(1204,0,53772,0,'Lord Rhyolith'),(1205,0,53691,0,'Shannox'),(1206,0,52530,0,'Alysrazor'),(1250,0,52363,0,'Occu\'thar'),(1268,1,72959,0,'Second Echo'),(1269,1,72959,0,'First Echo'),(1271,1,110158,435,'Murozond'),(1272,0,55085,0,'Peroth\'arn'),(1273,1,94981,0,'Queen Azshara'),(1274,1,105576,437,'Mannoroth'),(1291,1,104574,0,'Spine of Deathwing'),(1292,0,55265,0,'Morchok'),(1294,0,55308,0,'Warlord Zon\'ozz'),(1295,0,55312,0,'Yor\'sahj the Unsleeping'),(1296,0,55689,0,'Hagara'),(1297,0,55294,0,'Ultraxion'),(1298,0,56427,0,'Warmaster Blackhorn'),(1299,1,111533,447,'Madness of Deathwing'),(1332,0,55869,328,'Alizabal'),(1337,0,54590,0,'Arcurion'),(1339,0,54938,439,'Archbishop Benedictus'),(1340,0,54968,0,'Asira Dawnslayer'),(1348,0,16089,0,'Omar the Test Dragon');

INSERT INTO `instance_spawn_groups` VALUES (36,4,15,409,5),(36,4,15,410,9),(189,0,31,453,5),(189,0,31,454,9),(389,0,31,456,9),(645,2,8,434,1),(645,4,8,452,1),(657,0,17,403,1),(657,0,8,406,1),(657,1,17,404,1),(657,1,8,407,1),(657,2,17,405,1),(657,2,8,408,1),(671,0,17,439,1),(671,1,17,440,1),(671,2,17,441,1),(671,3,17,442,1),(720,0,17,447,1),(720,1,7,457,1),(720,2,17,444,1),(720,3,17,446,1),(720,4,17,445,1),(720,5,17,448,1),(720,6,17,449,1),(757,2,17,443,1),(938,4,17,438,1);

-- === 27_world_creature_extra_seed_spawns.sql ===
-- Spawns for creature_template seeds from migration 24 that had no `creature` rows.
-- Migration 25 only inserted the Kobold (entry 6, guid 9000001).

USE `firelands_world`;

UPDATE `creature_template`
SET `minlevel` = 25, `maxlevel` = 25, `unit_class` = 1
WHERE `entry` = 2575;

UPDATE `creature_template`
SET `minlevel` = 35, `maxlevel` = 35, `unit_class` = 8
WHERE `entry` = 35176;

INSERT IGNORE INTO `creature` (
  `guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `phaseUseFlags`,
  `PhaseId`, `PhaseGroup`, `terrainSwapMap`, `modelid`, `equipment_id`,
  `position_x`, `position_y`, `position_z`, `orientation`,
  `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curHealthPct`,
  `MovementType`, `npcflag`, `unit_flags`, `unit_flags2`, `unit_flags3`,
  `ScriptName`, `StringId`, `VerifiedBuild`
) VALUES
(
  9000002, 2575, 0, 9, 9, '0', 0,
  0, 0, -1, 0, 0,
  -8945.0, -128.0, 83.5, 5.0,
  120, 0, 0, 100,
  0, NULL, NULL, NULL, NULL,
  '', NULL, 0
),
(
  9000003, 35176, 0, 9, 9, '0', 0,
  0, 0, -1, 0, 0,
  -8942.0, -136.0, 83.5, 5.4,
  120, 0, 0, 100,
  0, NULL, NULL, NULL, NULL,
  '', NULL, 0
);
