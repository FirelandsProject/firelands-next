-- Remove custom item 900001 if it was inserted while GM starter used that entry.
-- Client tooltips need Item.db2 / ItemSparse; unknown IDs show "Retrieving item information".
-- GM starter pack is outfit-only (see `AppendGmStarterItems`); removes legacy 900001 row.

CREATE DATABASE IF NOT EXISTS `firelands_world`;
USE `firelands_world`;

DELETE FROM `firelands_world`.`item_template` WHERE `entry` = 900001;
