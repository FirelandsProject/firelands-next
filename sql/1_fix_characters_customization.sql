-- Customization columns are defined in `characters_schema.sql` for new installs.
-- This file remains as a no-op so existing deployments keep the same migration name
-- in `schema_migrations` without failing on duplicate ALTERs.
USE `firelands_characters`;
SELECT 1;

