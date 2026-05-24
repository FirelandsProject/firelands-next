-- Migration: persist which action bars are visible (PLAYER_FIELD_BYTES toggles byte)
-- Target: `firelands_characters`
-- JDBC-safe: skip ADD when init schema already includes the column.

CREATE DATABASE IF NOT EXISTS `firelands_characters`;
USE `firelands_characters`;

SET @exist_action_bar_toggles :=
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
   WHERE TABLE_SCHEMA = DATABASE()
     AND TABLE_NAME = 'characters'
     AND COLUMN_NAME = 'actionBarToggles');

SET @fl_sql := IF(@exist_action_bar_toggles = 0,
  'ALTER TABLE `characters`
     ADD COLUMN `actionBarToggles` tinyint unsigned NOT NULL DEFAULT ''255''
     AFTER `tutorial7`',
  'SELECT 1');

PREPARE _fl_m53_p FROM @fl_sql;
EXECUTE _fl_m53_p;
DEALLOCATE PREPARE _fl_m53_p;
