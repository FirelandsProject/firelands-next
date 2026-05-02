-- MariaDB (and some MySQL builds) reject DEFAULT on TEXT/BLOB columns.
-- `enchantments` is always supplied in application SQL (SaveInventory, GrantStarterItems).
-- No ALTER here; this file exists so migration numbering stays stable after the failed DEFAULT attempt.
SELECT 1;
