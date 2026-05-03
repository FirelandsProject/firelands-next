#!/usr/bin/env python3
"""
Create a new sql/migrations/<N>_<slug>.sql file with the next numeric prefix.

Usage:
  python3 tools/sql/new_migration.py add_guild_bank_tab
  python3 tools/sql/new_migration.py "Add guild bank tab" --db characters

Requires a short name (slug or phrase). Use --db to set the target database block.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
MIGRATIONS_DIR = REPO_ROOT / "sql" / "migrations"

DB_CONFIG = {
    "auth": ("firelands_auth", "Authentication / accounts schema."),
    "characters": ("firelands_characters", "Per-character persistence."),
    "world": ("firelands_world", "Static world data (items, creatures, spells, …)."),
}


def slugify(raw: str) -> str:
    s = raw.strip().lower().replace(" ", "_")
    s = re.sub(r"[^a-z0-9_]+", "_", s)
    s = re.sub(r"_+", "_", s).strip("_")
    if not s:
        print("Error: name produces an empty slug.", file=sys.stderr)
        sys.exit(1)
    if len(s) > 80:
        s = s[:80].rstrip("_")
    return s


def next_migration_number() -> int:
    highest = 0
    if not MIGRATIONS_DIR.is_dir():
        return 1
    for p in MIGRATIONS_DIR.glob("*.sql"):
        if not p.is_file():
            continue
        m = re.match(r"^(\d+)_", p.name)
        if m:
            highest = max(highest, int(m.group(1)))
    return highest + 1


def build_template(*, db_key: str, title: str, slug: str) -> str:
    db_name, db_hint = DB_CONFIG[db_key]
    return f"""-- Migration: {title}
-- Target: `{db_name}` — {db_hint}
--
-- Guidelines:
-- - Prefer idempotent DDL (IF NOT EXISTS, INFORMATION_SCHEMA checks before ALTER).
-- - Keep statements JDBC-friendly (avoid DELIMITER / stored procedures if possible).
-- - Regenerate docker bundle when appropriate: python3 tools/merge_migrations.py

CREATE DATABASE IF NOT EXISTS `{db_name}`;
USE `{db_name}`;

-- TODO: your schema or data changes below


"""


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Create a new numbered SQL migration under sql/migrations/."
    )
    parser.add_argument(
        "name",
        help="Short name, e.g. add_mail_index or \"Add mail index\"",
    )
    parser.add_argument(
        "--db",
        choices=list(DB_CONFIG.keys()),
        default="world",
        help="Logical database for CREATE DATABASE / USE (default: world).",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the path that would be created and exit.",
    )
    args = parser.parse_args()

    slug = slugify(args.name)
    n = next_migration_number()
    filename = f"{n}_{slug}.sql"
    path = MIGRATIONS_DIR / filename

    if path.exists():
        print(f"Error: file already exists: {path}", file=sys.stderr)
        sys.exit(1)

    title = args.name.strip()
    body = build_template(db_key=args.db, title=title, slug=slug)

    if args.dry_run:
        print(path.relative_to(REPO_ROOT))
        return

    MIGRATIONS_DIR.mkdir(parents=True, exist_ok=True)
    path.write_text(body, encoding="utf-8")
    print(f"Created {path.relative_to(REPO_ROOT)}")
    print("Next: edit the file, then run: python3 tools/merge_migrations.py")


if __name__ == "__main__":
    main()
