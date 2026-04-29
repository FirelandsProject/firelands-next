-- Privileges for the game DB user must be granted once by a DBA (or Docker init)
-- as a superuser, e.g.:
--   GRANT ALL PRIVILEGES ON firelands_auth.* TO 'firelands'@'%';
--   GRANT ALL PRIVILEGES ON firelands_characters.* TO 'firelands'@'%';
--   GRANT ALL PRIVILEGES ON firelands_world.* TO 'firelands'@'%';
--   FLUSH PRIVILEGES;
--
-- Running GRANT here would fail when migrations connect as `firelands`, so this
-- file is intentionally a no-op so it can be recorded in schema_migrations.
SELECT 1;
