-- Minimal quest tables for gossip quest lines (SMSG_GOSSIP_MESSAGE).
-- Full quest gameplay (accept/complete/status) is a separate milestone.
-- Data: migration 38 (`python3 tools/sql/import_ref_quest_gossip.py`).

USE `firelands_world`;

CREATE TABLE IF NOT EXISTS `quest_template` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `QuestLevel` smallint NOT NULL DEFAULT '1',
  `LogTitle` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Flags` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `creature_queststarter` (
  `id` mediumint unsigned NOT NULL DEFAULT '0' COMMENT 'Creature entry',
  `quest` int unsigned NOT NULL DEFAULT '0' COMMENT 'Quest Identifier',
  PRIMARY KEY (`id`, `quest`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
