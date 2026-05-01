CREATE DATABASE IF NOT EXISTS `firelands_characters`;
USE `firelands_characters`;

-- Runs after `characters_schema.sql` (lexicographic order). Idempotent: no-op if
-- `orientation` already exists (e.g. fresh installs from updated CREATE TABLE).

SET @exist :=
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
   WHERE TABLE_SCHEMA = DATABASE()
     AND TABLE_NAME = 'characters'
     AND COLUMN_NAME = 'orientation');

-- CONCAT avoids escaped single-quotes inside literals (splitter-safe either way).
SET @sqlstmt := IF(@exist = 0,
  CONCAT('ALTER TABLE `characters` ADD COLUMN `orientation` float NOT NULL DEFAULT 0 AFTER `z`'),
  'SELECT 1');

PREPARE stmt FROM @sqlstmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
