-- Clears migration bookkeeping so the world server can re-apply sql/bundled + sql/init.
-- Does NOT drop game data (characters, world tables). For a full wipe use:
--   tools/sql/docker_db_fresh_start.sh
--
-- Run (Docker):
--   docker compose exec -T db mysql -uroot -proot < tools/sql/clear_schema_migrations.sql
--   docker compose exec -T db mysql -uroot -proot < sql/bundled/zz_seed_schema_migrations.sql
--
-- Run (local mysql client):
--   mysql -h127.0.0.1 -uroot -proot < tools/sql/clear_schema_migrations.sql

USE `firelands_auth`;

TRUNCATE TABLE `schema_migrations`;
