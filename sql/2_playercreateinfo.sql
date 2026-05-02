CREATE DATABASE IF NOT EXISTS `firelands_world`;
USE `firelands_world`;

-- Minimal player creation templates (Cata 4.3.4). We keep these in world DB
-- because they're static reference data (like Trinity's playercreateinfo_*).

CREATE TABLE IF NOT EXISTS `playercreateinfo_stats` (
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',   -- 0 = wildcard
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',  -- 0 = wildcard
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
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',   -- 0 = wildcard
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',  -- 0 = wildcard
  `spellId` int(10) unsigned NOT NULL,
  PRIMARY KEY (`race`, `class`, `spellId`),
  KEY `idx_class` (`class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `playercreateinfo_skill` (
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',   -- 0 = wildcard
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',  -- 0 = wildcard
  `skillId` smallint(5) unsigned NOT NULL,
  `rank` smallint(5) unsigned NOT NULL DEFAULT '0',
  `maxRank` smallint(5) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`race`, `class`, `skillId`),
  KEY `idx_class` (`class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `playercreateinfo_faction` (
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',   -- 0 = wildcard
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',  -- 0 = wildcard
  `reputationListId` smallint(5) unsigned NOT NULL,  -- 0..255
  `flags` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `standing` int(10) NOT NULL DEFAULT '0',
  PRIMARY KEY (`race`, `class`, `reputationListId`),
  KEY `idx_class` (`class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `playercreateinfo_reputation` (
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',   -- 0 = wildcard
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',  -- 0 = wildcard
  `minLevel` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `maxLevel` tinyint(3) unsigned NOT NULL DEFAULT '255',
  `factionId` int(10) unsigned NOT NULL,             -- Faction.dbc ID
  `flags` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `standing` int(10) NOT NULL DEFAULT '0',
  PRIMARY KEY (`race`, `class`, `minLevel`, `maxLevel`, `factionId`),
  KEY `idx_race_class` (`race`, `class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ---- SEEDS (idempotent) ----
-- Stats (race=0 wildcard). These are intentionally "starter-ish" values to avoid
-- empty character sheets — you can replace later with exact FirelandsCore numbers.
INSERT IGNORE INTO `playercreateinfo_stats` (`race`, `class`, `str`, `agi`, `sta`, `intel`, `spi`, `maxHealth`, `maxMana`) VALUES
  (0, 1, 23, 20, 22, 18, 19, 100, 0),   -- Warrior
  (0, 2, 22, 18, 21, 20, 20, 100, 100), -- Paladin
  (0, 3, 18, 23, 20, 19, 20, 100, 0),   -- Hunter (focus)
  (0, 4, 19, 24, 20, 18, 19, 100, 0),   -- Rogue (energy)
  (0, 5, 17, 18, 19, 24, 23, 100, 100), -- Priest
  (0, 6, 23, 18, 22, 18, 18, 100, 0),   -- Death Knight (runic)
  (0, 7, 19, 19, 20, 22, 21, 100, 100), -- Shaman
  (0, 8, 17, 18, 19, 25, 22, 100, 100), -- Mage
  (0, 9, 17, 18, 20, 24, 20, 100, 100), -- Warlock
  (0,11, 18, 19, 20, 22, 22, 100, 100); -- Druid

-- Languages as skills. Add both Common and Orcish to avoid client-side chat lockouts.
INSERT IGNORE INTO `playercreateinfo_skill` (`race`, `class`, `skillId`, `rank`, `maxRank`) VALUES
  (0, 0, 98, 300, 300),  -- Language: Common
  (0, 0, 109, 300, 300); -- Language: Orcish

-- Starter spells per class (race=0 wildcard), mirroring the previous hardcoded fallback.
-- Also seed both language spells.
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

-- Starter reputations by race (home city coalition), factionId based (Faction.dbc).
-- flags: 0x01 Visible (keeps reputation UI populated with the correct subset).
INSERT IGNORE INTO `playercreateinfo_reputation` (`race`, `class`, `minLevel`, `maxLevel`, `factionId`, `flags`, `standing`) VALUES
  -- Alliance coalition
  (1, 0, 1, 255, 72,  1, 0),  -- Stormwind
  (1, 0, 1, 255, 47,  1, 0),  -- Ironforge
  (1, 0, 1, 255, 69,  1, 0),  -- Darnassus
  (1, 0, 1, 255, 54,  1, 0),  -- Gnomeregan
  (1, 0, 1, 255, 930, 1, 0),  -- Exodar
  -- Horde coalition
  (2, 0, 1, 255, 76,  1, 0),  -- Orgrimmar
  (2, 0, 1, 255, 81,  1, 0),  -- Thunder Bluff
  (2, 0, 1, 255, 68,  1, 0),  -- Undercity
  (2, 0, 1, 255, 530, 1, 0),  -- Darkspear Trolls
  (2, 0, 1, 255, 911, 1, 0);  -- Silvermoon City

