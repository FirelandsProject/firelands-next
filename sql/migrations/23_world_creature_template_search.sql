-- Minimal creature_template for GM `.npc search` (name lookup).
-- Populate from your Cataclysm client DB export or tooling; seed rows are examples only.

USE `firelands_world`;

CREATE TABLE IF NOT EXISTS `creature_template` (
  `entry` int unsigned NOT NULL,
  `name` varchar(100) NOT NULL DEFAULT '',
  `subname` varchar(100) NOT NULL DEFAULT '',
  PRIMARY KEY (`entry`),
  KEY `idx_creature_template_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `creature_template` (`entry`, `name`, `subname`) VALUES
  (6, 'Kobold Vermin', 'Monster'),
  (2575, 'Harlan Bagley', 'Baker'),
  (35176, 'Stormwind Mage', 'Portal Trainer')
ON DUPLICATE KEY UPDATE
  `name` = VALUES(`name`),
  `subname` = VALUES(`subname`);
