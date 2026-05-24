-- Rested XP pool (Cataclysm `PLAYER_REST_STATE_EXPERIENCE` / rest indicator).
-- JDBC-safe: skip ADD when init schema already includes the column.
USE `firelands_characters`;

SET @exist_rest_bonus :=
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
   WHERE TABLE_SCHEMA = DATABASE()
     AND TABLE_NAME = 'characters'
     AND COLUMN_NAME = 'rest_bonus');

SET @fl_sql := IF(@exist_rest_bonus = 0,
  'ALTER TABLE `characters`
     ADD COLUMN `rest_bonus` float NOT NULL DEFAULT 0 AFTER `xp`',
  'SELECT 1');

PREPARE _fl_m54_p FROM @fl_sql;
EXECUTE _fl_m54_p;
DEALLOCATE PREPARE _fl_m54_p;
