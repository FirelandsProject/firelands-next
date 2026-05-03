#!/usr/bin/env bash
# Regenerate merged SQL, destroy the Docker MySQL volume, and start a clean database.
# Requires: docker compose, python3.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "[1/3] Refreshing sql/bundled/ (merge_migrations.py; migrations/*.sql optional) ..."
python3 tools/merge_migrations.py

echo "[2/3] Stopping db and removing persisted data (volume) ..."
docker compose down -v

echo "[3/3] Starting MySQL (first boot applies docker-entrypoint-initdb.d scripts) ..."
docker compose up -d db

echo "Waiting for MySQL to accept connections ..."
for _ in $(seq 1 90); do
  if docker compose exec -T db mysqladmin ping -h 127.0.0.1 -uroot -proot --silent 2>/dev/null; then
    echo "MySQL is up. Databases firelands_auth / firelands_characters / firelands_world are ready."
    exit 0
  fi
  sleep 1
done

echo "Timed out waiting for MySQL." >&2
exit 1
