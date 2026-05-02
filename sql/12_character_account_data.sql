-- Legacy filename kept for migration bookkeeping.
-- `character_account_data` is created in `characters_schema.sql` (after `characters`,
-- so the FK is valid). This file used to run *before* `characters_schema.sql` in
-- lexicographic order and failed on the FK; do not recreate the table here.
SELECT 1;
