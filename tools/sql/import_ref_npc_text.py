#!/usr/bin/env python3
"""
Import reference-compatible `npc_text` rows from firelands-cata-ref into Firelands Next.

Reads reference mysqldump INSERTs and emits a JDBC-safe world migration:
  USE `firelands_world`;
  DELETE FROM `npc_text`;
  batched REPLACE INTO `npc_text` (...)

Text columns are normalized to Firelands `N'...'` literals (utf8mb4). Numeric and
emote fields are passed through unchanged. `BroadcastTextID*` is kept for future
broadcast_text wiring; the server currently serves `text*_*` only.

Usage:
  python3 tools/sql/import_ref_npc_text.py
  python3 tools/sql/import_ref_npc_text.py --ref /path/to/firelands-cata-ref \\
      --out sql/migrations/34_world_npc_text_data.sql
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parents[2]
_TOOLS_SQL = Path(__file__).resolve().parent
if str(_TOOLS_SQL) not in sys.path:
    sys.path.insert(0, str(_TOOLS_SQL))

    from import_ref_creature_data import ( # noqa: E402
    extract_insert_rows,
    sql_escape_literal,
    strip_sql_string,
    write_batched,
    )

_FIELDS_PER_ROW = 90
_OPTIONS = 8
_FIELDS_PER_OPTION = 11


def build_npc_text_column_list() -> str:
    parts: list[str] = ["`ID`"]
    for i in range(_OPTIONS):
        parts.extend(
            [
                f"`text{i}_0`",
                f"`text{i}_1`",
                f"`BroadcastTextID{i}`",
                f"`lang{i}`",
                f"`Probability{i}`",
                f"`EmoteDelay{i}_0`",
                f"`Emote{i}_0`",
                f"`EmoteDelay{i}_1`",
                f"`Emote{i}_1`",
                f"`EmoteDelay{i}_2`",
                f"`Emote{i}_2`",
            ]
    )
    parts.append("`VerifiedBuild`")
    return ", ".join(parts)


NPC_TEXT_COLUMNS = build_npc_text_column_list()


def sql_text_column(tok: str) -> str:
    """Reference longtext → Firelands `N'...'` or SQL NULL."""
    tok = tok.strip()
    if tok.upper() == "NULL":
        return "NULL"
    if tok == "''":
        return "N''"
    if tok.startswith("'"):
        return "N'" + sql_escape_literal(strip_sql_string(tok)) + "'"
    return tok


def sql_verified_build(tok: str) -> str:
    tok = tok.strip()
    if tok.upper() == "NULL":
        return "NULL"
    return tok


def map_npc_text_row(fields: list[str]) -> str:
    if len(fields) != _FIELDS_PER_ROW:
        raise ValueError(
            f"npc_text row expected {_FIELDS_PER_ROW} fields, got {len(fields)}"
    )

    vals: list[str] = [fields[0].strip()]
    for opt in range(_OPTIONS):
        base = 1 + opt * _FIELDS_PER_OPTION
        vals.append(sql_text_column(fields[base]))
        vals.append(sql_text_column(fields[base + 1]))
        for j in range(2, _FIELDS_PER_OPTION):
            vals.append(fields[base + j].strip())
    vals.append(sql_verified_build(fields[-1]))
    return "(" + ",".join(vals) + ")"


def write_npc_text_data_migration(
    npc_text_sql: Path,
    out_path: Path,
    *,
    batch_size: int = 40,
) -> None:
    rows = extract_insert_rows(npc_text_sql, "npc_text")
    mapped = [map_npc_text_row(r) for r in rows]

    header = (
        "-- NPC gossip page copy (`npc_text`) from firelands-cata-ref.\n"
  "-- Reference: `npc_text` (read-only); Firelands serves text*_* on\n"
        "-- CMSG_NPC_TEXT_QUERY. BroadcastTextID* retained for future broadcast_text.\n"
        "-- JDBC-safe: DELETE + batched REPLACE (re-runnable).\n"
        "-- Regenerate: python3 tools/sql/import_ref_npc_text.py\n"
        "-- Requires migration 33_world_npc_text.sql (table DDL).\n"
        "\n"
        "USE `firelands_world`;\n"
        "\n"
        "DELETE FROM `npc_text`;\n"
        "\n"
    )

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as out:
        out.write(header)
        write_batched(
            out,
            f"REPLACE INTO `npc_text` ({NPC_TEXT_COLUMNS}) VALUES",
            mapped,
            batch_size,
    )

    mib = out_path.stat().st_size / (1024 * 1024)
    batches = (len(mapped) + batch_size - 1) // batch_size if mapped else 0
    print(
        f"Wrote {out_path.name}: {len(mapped)} rows, {batches} batch(es), {mib:.2f} MiB"
    )


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--ref",
        type=Path,
        default=_REPO_ROOT / "firelands-cata-ref",
        help="Path to firelands-cata-ref checkout",
    )
    ap.add_argument(
        "--out",
        type=Path,
        default=_REPO_ROOT / "sql" / "migrations" / "34_world_npc_text_data.sql",
        help="Output migration SQL path",
    )
    ap.add_argument(
        "--batch-size",
        type=int,
        default=40,
        help="Rows per REPLACE statement (lower if max_allowed_packet errors)",
    )
    args = ap.parse_args()

    npc_text_sql = args.ref / "data" / "sql" / "base" / "db_world" / "npc_text.sql"
    if not npc_text_sql.is_file():
        raise SystemExit(f"Missing {npc_text_sql}")

    print(f"Parsing {npc_text_sql.name} ...")
    write_npc_text_data_migration(
        npc_text_sql, args.out, batch_size=args.batch_size
    )


if __name__ == "__main__":
    main()
