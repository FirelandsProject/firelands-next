#!/usr/bin/env python3
"""
Import Trinity-compatible gossip tables from firelands-cata-ref into Firelands Next.

Emits a JDBC-safe world migration:
  USE `firelands_world`;
  DELETE FROM gossip_* (child tables first);
  batched REPLACE INTO gossip_menu, gossip_menu_option,
    gossip_menu_option_action, gossip_menu_option_box

Column layout matches migration 32 / world_schema (Trinity Cata 4.3.4).
Text columns use Firelands `N'...'` literals.

Usage:
  python3 tools/sql/import_ref_gossip.py
  python3 tools/sql/import_ref_gossip.py --ref /path/to/firelands-cata-ref \\
      --out sql/migrations/35_world_gossip_data.sql
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parents[2]
_TOOLS_SQL = Path(__file__).resolve().parent
if str(_TOOLS_SQL) not in sys.path:
    sys.path.insert(0, str(_TOOLS_SQL))

from import_ref_creature_data import (  # noqa: E402
    extract_insert_rows,
    sql_escape_literal,
    strip_sql_string,
    write_batched,
)

GOSSIP_MENU_COLUMNS = "`MenuID`, `TextID`, `VerifiedBuild`"

GOSSIP_MENU_OPTION_COLUMNS = (
    "`MenuId`, `OptionIndex`, `OptionIcon`, `OptionText`, "
    "`OptionBroadcastTextId`, `OptionType`, `OptionNpcflag`, `VerifiedBuild`"
)

GOSSIP_MENU_OPTION_ACTION_COLUMNS = (
    "`MenuId`, `OptionIndex`, `ActionMenuId`, `ActionPoiId`"
)

GOSSIP_MENU_OPTION_BOX_COLUMNS = (
    "`MenuId`, `OptionIndex`, `BoxCoded`, `BoxMoney`, `BoxText`, `BoxBroadcastTextId`"
)


def sql_text_column(tok: str) -> str:
    tok = tok.strip()
    if tok.upper() == "NULL":
        return "NULL"
    if tok == "''":
        return "N''"
    if tok.startswith("'"):
        return "N'" + sql_escape_literal(strip_sql_string(tok)) + "'"
    return tok


def map_gossip_menu_row(fields: list[str]) -> str:
    if len(fields) != 3:
        raise ValueError(f"gossip_menu row expected 3 fields, got {len(fields)}")
    return "(" + ",".join(f.strip() for f in fields) + ")"


def map_gossip_menu_option_row(fields: list[str]) -> str:
    if len(fields) != 8:
        raise ValueError(f"gossip_menu_option row expected 8 fields, got {len(fields)}")
    vals = [
        fields[0].strip(),
        fields[1].strip(),
        fields[2].strip(),
        sql_text_column(fields[3]),
        fields[4].strip(),
        fields[5].strip(),
        fields[6].strip(),
        fields[7].strip(),
    ]
    return "(" + ",".join(vals) + ")"


def map_gossip_menu_option_action_row(fields: list[str]) -> str:
    if len(fields) != 4:
        raise ValueError(
            f"gossip_menu_option_action row expected 4 fields, got {len(fields)}"
        )
    return "(" + ",".join(f.strip() for f in fields) + ")"


def map_gossip_menu_option_box_row(fields: list[str]) -> str:
    if len(fields) != 6:
        raise ValueError(
            f"gossip_menu_option_box row expected 6 fields, got {len(fields)}"
        )
    vals = [
        fields[0].strip(),
        fields[1].strip(),
        fields[2].strip(),
        fields[3].strip(),
        sql_text_column(fields[4]),
        fields[5].strip(),
    ]
    return "(" + ",".join(vals) + ")"


def write_gossip_data_migration(ref_dir: Path, out_path: Path) -> None:
    menu_sql = ref_dir / "gossip_menu.sql"
    option_sql = ref_dir / "gossip_menu_option.sql"
    action_sql = ref_dir / "gossip_menu_option_action.sql"
    box_sql = ref_dir / "gossip_menu_option_box.sql"

    for path in (menu_sql, option_sql, action_sql, box_sql):
        if not path.is_file():
            raise SystemExit(f"Missing {path}")

    print("Parsing gossip_menu...")
    menu_rows = [map_gossip_menu_row(r) for r in extract_insert_rows(menu_sql, "gossip_menu")]
    print("Parsing gossip_menu_option...")
    option_rows = [
        map_gossip_menu_option_row(r)
        for r in extract_insert_rows(option_sql, "gossip_menu_option")
    ]
    print("Parsing gossip_menu_option_action...")
    action_rows = [
        map_gossip_menu_option_action_row(r)
        for r in extract_insert_rows(action_sql, "gossip_menu_option_action")
    ]
    print("Parsing gossip_menu_option_box...")
    box_rows = [
        map_gossip_menu_option_box_row(r)
        for r in extract_insert_rows(box_sql, "gossip_menu_option_box")
    ]

    header = (
        "-- Gossip menus and options from firelands-cata-ref (Trinity 4.3.4 layout).\n"
        "-- Used by IGossipRepository / SMSG_GOSSIP_MESSAGE + menu chaining.\n"
        "-- JDBC-safe: DELETE + batched REPLACE (re-runnable).\n"
        "-- Regenerate: python3 tools/sql/import_ref_gossip.py\n"
        "-- Requires migration 32_world_gossip_tables.sql (DDL).\n"
        "\n"
        "USE `firelands_world`;\n"
        "\n"
        "DELETE FROM `gossip_menu_option_box`;\n"
        "DELETE FROM `gossip_menu_option_action`;\n"
        "DELETE FROM `gossip_menu_option`;\n"
        "DELETE FROM `gossip_menu`;\n"
        "\n"
    )

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as out:
        out.write(header)
        write_batched(
            out,
            f"REPLACE INTO `gossip_menu` ({GOSSIP_MENU_COLUMNS}) VALUES",
            menu_rows,
            250,
        )
        write_batched(
            out,
            f"REPLACE INTO `gossip_menu_option` ({GOSSIP_MENU_OPTION_COLUMNS}) VALUES",
            option_rows,
            120,
        )
        write_batched(
            out,
            f"REPLACE INTO `gossip_menu_option_action` ({GOSSIP_MENU_OPTION_ACTION_COLUMNS}) VALUES",
            action_rows,
            200,
        )
        write_batched(
            out,
            f"REPLACE INTO `gossip_menu_option_box` ({GOSSIP_MENU_OPTION_BOX_COLUMNS}) VALUES",
            box_rows,
            96,
        )

    mib = out_path.stat().st_size / (1024 * 1024)
    print(
        f"Wrote {out_path.name}: menus={len(menu_rows)} options={len(option_rows)} "
        f"actions={len(action_rows)} boxes={len(box_rows)} ({mib:.2f} MiB)"
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
        default=_REPO_ROOT / "sql" / "migrations" / "35_world_gossip_data.sql",
        help="Output migration SQL path",
    )
    args = ap.parse_args()

    ref_db_world = args.ref / "data" / "sql" / "base" / "db_world"
    if not ref_db_world.is_dir():
        raise SystemExit(f"Missing ref db_world directory: {ref_db_world}")

    write_gossip_data_migration(ref_db_world, args.out)


if __name__ == "__main__":
    main()
