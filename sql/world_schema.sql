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
