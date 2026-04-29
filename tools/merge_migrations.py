#!/usr/bin/env python3
"""
Merge all .sql migration files under an input directory into one script per
target database (detected from USE `db` or CREATE DATABASE ... `db`).

Unclassified files (no database reference) are written to _unclassified.sql.

`0_init_permissions.sql` is never merged: it stays only under the input dir
(e.g. sql/) so it is not copied into the output folder.

Usage:
  python3 tools/merge_migrations.py [--input-dir sql] [--output-dir sql/merged]
"""
from __future__ import annotations

import argparse
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

USE_RE = re.compile(
    r"(?is)\bUSE\s+`([^`]+)`",
)
CREATE_DB_RE = re.compile(
    r"(?is)\bCREATE\s+DATABASE\s+(?:IF\s+NOT\s+EXISTS\s+)?`([^`]+)`",
)

# Must remain only in sql/; never written to --output-dir (see project 0_init_permissions.sql).
STANDALONE_INIT_SQL = frozenset({"0_init_permissions.sql"})


def detect_database(sql: str) -> str | None:
    """Pick primary DB: first USE wins, else first CREATE DATABASE."""
    m = USE_RE.search(sql)
    if m:
        return m.group(1).strip()
    m = CREATE_DB_RE.search(sql)
    if m:
        return m.group(1).strip()
    return None


def safe_db_filename(db: str) -> str:
    out = "".join(c if c.isalnum() or c in "_-" else "_" for c in db)
    return out or "empty"


def main() -> int:
    p = argparse.ArgumentParser(
        description="Condense SQL migrations into one file per database.",
    )
    p.add_argument(
        "--input-dir",
        type=Path,
        default=Path("sql"),
        help="Directory containing .sql migration files (default: sql)",
    )
    p.add_argument(
        "--output-dir",
        type=Path,
        default=Path("sql/merged"),
        help="Where merged scripts are written (default: sql/merged)",
    )
    p.add_argument(
        "--dry-run",
        action="store_true",
        help="Print plan only; do not write files",
    )
    args = p.parse_args()
    input_dir: Path = args.input_dir.resolve()
    output_dir: Path = args.output_dir.resolve()

    if not input_dir.is_dir():
        print(f"error: input directory does not exist: {input_dir}", file=sys.stderr)
        return 1

    sql_files = sorted(
        f for f in input_dir.iterdir() if f.is_file() and f.suffix.lower() == ".sql"
    )
    if not sql_files:
        print(f"warning: no .sql files in {input_dir}", file=sys.stderr)
        return 0

    # db_name -> list of (path, content)
    buckets: dict[str, list[tuple[Path, str]]] = {}
    standalone: list[Path] = []

    for path in sql_files:
        if path.name in STANDALONE_INIT_SQL:
            standalone.append(path)
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except OSError as e:
            print(f"error: cannot read {path}: {e}", file=sys.stderr)
            return 1
        db = detect_database(text)
        key = db if db else "_unclassified"
        buckets.setdefault(key, []).append((path, text))

    iso = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    plan: list[tuple[str, list[Path]]] = [
        (db, [p for p, _ in items]) for db, items in sorted(buckets.items())
    ]

    print("Merge plan:")
    for db, paths in plan:
        print(f"  {db}: {[p.name for p in paths]}")
    for p in standalone:
        print(f"  (standalone, stays in {input_dir.name}/): {p.name}")

    if args.dry_run:
        return 0

    output_dir.mkdir(parents=True, exist_ok=True)

    for db, items in buckets.items():
        fname = f"{safe_db_filename(db)}.sql" if db != "_unclassified" else "_unclassified.sql"
        out_path = output_dir / fname
        parts: list[str] = [
            f"-- Merged SQL for database `{db}`" if db != "_unclassified"
            else "-- Merged SQL (no USE / CREATE DATABASE detected in source files)",
            f"-- Generated: {iso}",
            "-- Sources (same order as DatabaseMigrator: all sql/*.sql sorted by filename):",
        ]
        for p, _ in items:
            parts.append(f"--   - {p.name}")
        parts.append("")
        parts.append("SET NAMES utf8mb4;")
        parts.append("SET FOREIGN_KEY_CHECKS=0;")
        parts.append("")

        for p, text in items:
            parts.append(f"-- >>> begin: {p.name}")
            parts.append(text.rstrip())
            parts.append("")
            parts.append(f"-- <<< end: {p.name}")
            parts.append("")

        parts.append("SET FOREIGN_KEY_CHECKS=1;")
        parts.append("")

        body = "\n".join(parts)
        try:
            out_path.write_text(body, encoding="utf-8")
        except OSError as e:
            print(f"error: cannot write {out_path}: {e}", file=sys.stderr)
            return 1
        print(f"wrote {out_path}")

    # Drop stale _unclassified.sql if nothing maps there anymore (e.g. only init was unclassified).
    stale_uncl = output_dir / "_unclassified.sql"
    if "_unclassified" not in buckets and stale_uncl.is_file():
        try:
            stale_uncl.unlink()
            print(f"removed stale {stale_uncl}")
        except OSError as e:
            print(f"warning: could not remove {stale_uncl}: {e}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    sys.exit(main())
