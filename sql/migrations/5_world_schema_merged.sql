-- Merged world migrations (Firelands Next)
-- Execution order: lexicographic in DatabaseMigrator; this bundle mirrors 2..16 + 17 + z_ensure

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
-- Parity with firelands-cata-ref `data/sql/base/db_hotfixes/item.sql` (DB2 Item.db2):
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
  `MvpDirectHealthDelta` int DEFAULT NULL COMMENT 'Phase D MVP: direct health delta on hit, NULL = none',
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
-- sql/migrations/19_world_spell_dbc_mvp_direct_health.sql (MvpDirectHealthDelta for Phase D MVP).

-- === z_ensure_player_classlevelstats_seed.sql (idempotent seed) ===
INSERT IGNORE INTO `player_classlevelstats` (`class`, `level`, `str`, `agi`, `sta`, `inte`, `spi`) VALUES
  (1,1,23,20,22,20,21), (2,1,23,20,22,20,22), (3,1,22,21,22,20,21),
  (4,1,23,21,21,20,21), (5,1,17,22,22,22,23), (6,1,25,19,22,20,22),
  (7,1,22,21,22,20,22), (8,1,17,22,22,23,23), (9,1,21,21,22,23,23), (11,1,22,20,22,22,23);

INSERT IGNORE INTO `player_racestats` (`race`, `str`, `agi`, `sta`, `inte`, `spi`) VALUES
  (1,0,0,0,0,0), (2,3,-3,3,-3,0), (3,0,0,1,0,0), (4,-4,2,0,0,0),
  (5,0,0,0,0,0), (6,1,0,1,0,0), (7,-5,2,0,3,0), (8,1,2,0,0,0),
  (9,0,0,0,0,0), (10,0,0,0,0,0), (11,0,0,0,2,0), (22,0,0,0,0,0);