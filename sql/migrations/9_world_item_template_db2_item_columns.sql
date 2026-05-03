-- Align `item_template` with reference implementation `data/sql/base/db_hotfixes/item.sql`
-- (DB2 Item.db2 column semantics). Idempotent: safe if columns already exist.
-- Note: `DatabaseMigrator` skips `USE` lines; use explicit `firelands_world` in checks.

CREATE DATABASE IF NOT EXISTS `firelands_world`;
USE `firelands_world`;

SET @__col_exists := (
  SELECT COUNT(*) FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = 'firelands_world' AND TABLE_NAME = 'item_template'
    AND COLUMN_NAME = 'sound_override_subclass'
);
SET @__sql := IF(
  @__col_exists = 0,
  'ALTER TABLE `firelands_world`.`item_template` ADD COLUMN `sound_override_subclass` int NOT NULL DEFAULT 0 AFTER `subclass`',
  'SELECT 1 WHERE 0'
);
PREPARE __stmt FROM @__sql;
EXECUTE __stmt;
DEALLOCATE PREPARE __stmt;

SET @__col_exists := (
  SELECT COUNT(*) FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = 'firelands_world' AND TABLE_NAME = 'item_template'
    AND COLUMN_NAME = 'verified_build'
);
SET @__sql := IF(
  @__col_exists = 0,
  'ALTER TABLE `firelands_world`.`item_template` ADD COLUMN `verified_build` smallint NOT NULL DEFAULT 0 AFTER `SheatheType`',
  'SELECT 1 WHERE 0'
);
PREPARE __stmt FROM @__sql;
EXECUTE __stmt;
DEALLOCATE PREPARE __stmt;

ALTER TABLE `firelands_world`.`item_template`
  MODIFY COLUMN `Material` int NOT NULL DEFAULT 0;
