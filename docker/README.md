# Docker usage

## Database

```bash
docker compose up -d db
```

Wait until healthy (`docker compose ps`). First boot runs:

- `firelands_auth`, `firelands_characters`, `firelands_world` base schemas
- `docker/mysql-init/docker_grants_firelands_user.sql` as **root** so user `firelands` can use those DBs (world migrations never run GRANT — they connect as `firelands`)

If you ever create the DB without Docker init, apply that grants file once as MySQL root.

### JDBC from the host

Use host `127.0.0.1` and port `3306`, same URIs as in `worldserver.yaml`:

- `jdbc:mariadb://127.0.0.1:3306/firelands_auth`
- `jdbc:mariadb://127.0.0.1:3306/firelands_characters`
- `jdbc:mariadb://127.0.0.1:3306/firelands_world`

## Apply SQL migrations (optional)

If you need everything under `sql/` applied without starting `world` (or after pulling new migrations):

```bash
docker compose --profile migrate run --rm migrate
```

Runs as MySQL **root** so `CREATE DATABASE` in migrations succeeds. The world server still maintains `firelands_auth.schema_migrations` when you run it later.

## CharStartOutfit → SQL `IN` list

Place `CharStartOutfit.dbc` at `./data/dbc/CharStartOutfit.dbc` (or pass another path):

```bash
docker compose --profile tools run --rm extract-charstart-items data/dbc/CharStartOutfit.dbc --sql-in
```

One ID per line (no `--sql-in`):

```bash
docker compose --profile tools run --rm extract-charstart-items data/dbc/CharStartOutfit.dbc
```

## Backfill `item_proto_cache`

1. Apply migration `sql/9_firelands_world_item_template.sql` (via migrate profile or world startup) so `firelands_world.item_template` exists.
2. Load item rows into `item_template` (INSERT scripts from your world data pack, or import a dump — full dumps usually replace this table with the complete schema).
3. Run the backfill (Option A copies all rows from `item_template` into `item_proto_cache`):

```bash
docker compose exec -T db mysql -uroot -proot firelands_world < tools/backfill_item_proto_cache.sql
```

Use Option B in that file for CharStartOutfit-only `entry IN (...)`.

## Reset data volume

```bash
docker compose down -v
```

Next `docker compose up -d db` re-runs init scripts.
