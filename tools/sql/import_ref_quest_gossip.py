#!/usr/bin/env python3
"""
Import Trinity-compatible quest gossip data from firelands-cata-ref into Firelands Next.

Reads reference mysqldump INSERTs and emits a JDBC-safe world migration:
  USE `firelands_world`;
  DELETE FROM `creature_queststarter`;
  DELETE FROM `quest_template`;
  batched REPLACE INTO `quest_template`
    (ID, QuestLevel, LogTitle, Flags, AllowableClasses, AllowableRaces) for quests
    referenced by starters (`Allowable*` from `quest_template_addon`);
  batched REPLACE INTO `creature_queststarter` (id, quest)

Usage:
  python3 tools/sql/import_ref_quest_gossip.py
  python3 tools/sql/import_ref_quest_gossip.py --ref /path/to/firelands-cata-ref \\
      --out sql/migrations/38_world_quest_gossip_data.sql
"""

from __future__ import annotations

import argparse
import re
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

QUEST_TEMPLATE_COLUMNS = (
    "`ID`, `QuestLevel`, `LogTitle`, `Flags`, `AllowableClasses`, `AllowableRaces`"
)
CREATURE_QUESTSTARTER_COLUMNS = "`id`, `quest`"


def extract_create_table_columns(sql_path: Path, table: str) -> list[str]:
    """Return column names in order from a reference CREATE TABLE block."""
    cols: list[str] = []
    in_create = False
    with sql_path.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if re.search(rf"CREATE TABLE\s+`?{re.escape(table)}`?\s*\(", line, re.I):
                in_create = True
                continue
            if not in_create:
                continue
            stripped = line.strip()
            if stripped.startswith("`"):
                col = stripped.split("`", 2)[1]
                cols.append(col)
            if stripped.startswith(")") or stripped.upper().startswith("PRIMARY KEY"):
                if cols:
                    break
    if not cols:
        raise SystemExit(f"Could not parse CREATE TABLE columns for `{table}` in {sql_path}")
    return cols


def sql_text_column(tok: str) -> str:
    tok = tok.strip()
    if tok.upper() == "NULL":
        return "NULL"
    if tok == "''":
        return "N''"
    if tok.startswith("'"):
        return "N'" + sql_escape_literal(strip_sql_string(tok)) + "'"
    return tok


def map_creature_queststarter_row(fields: list[str]) -> str:
    if len(fields) != 2:
        raise ValueError(
            f"creature_queststarter row expected 2 fields, got {len(fields)}"
        )
    return f"({fields[0].strip()},{fields[1].strip()})"


def load_quest_addon_masks(ref_dir: Path) -> dict[int, tuple[int, int]]:
    """Quest id -> (AllowableClasses, AllowableRaces) from `quest_template_addon`."""
    addon_sql = ref_dir / "quest_template_addon.sql"
    if not addon_sql.is_file():
        raise SystemExit(f"Missing {addon_sql}")

    cols = extract_create_table_columns(addon_sql, "quest_template_addon")
    col_index = {name: i for i, name in enumerate(cols)}
    for required in ("ID", "AllowableClasses", "AllowableRaces"):
        if required not in col_index:
            raise SystemExit(
                f"quest_template_addon missing column {required!r}; found: {cols[:12]}..."
            )

    masks: dict[int, tuple[int, int]] = {}
    for row in extract_insert_rows(addon_sql, "quest_template_addon"):
        quest_id = int(row[col_index["ID"]].strip())
        classes = row[col_index["AllowableClasses"]].strip()
        races = row[col_index["AllowableRaces"]].strip()
        if classes.upper() == "NULL":
            classes = "0"
        if races.upper() == "NULL":
            races = "0"
        masks[quest_id] = (int(classes), int(races))
    return masks


def map_quest_template_row(
    fields: list[str],
    col_index: dict[str, int],
    wanted_ids: set[int],
    addon_masks: dict[int, tuple[int, int]],
) -> str | None:
    idx_id = col_index["ID"]
    quest_id = int(fields[idx_id].strip())
    if quest_id not in wanted_ids:
        return None

    idx_level = col_index["QuestLevel"]
    idx_title = col_index["LogTitle"]
    idx_flags = col_index["Flags"]

    level_tok = fields[idx_level].strip()
    if level_tok.upper() == "NULL":
        level_sql = "1"
    else:
        level_sql = level_tok

    title_sql = sql_text_column(fields[idx_title])
    flags_sql = fields[idx_flags].strip()
    if flags_sql.upper() == "NULL":
        flags_sql = "0"

    allowable_classes, allowable_races = addon_masks.get(quest_id, (0, 0))

    return (
        f"({quest_id},{level_sql},{title_sql},{flags_sql},"
        f"{allowable_classes},{allowable_races})"
    )


def write_quest_gossip_data_migration(ref_dir: Path, out_path: Path) -> None:
    starter_sql = ref_dir / "creature_queststarter.sql"
    quest_sql = ref_dir / "quest_template.sql"

    for path in (starter_sql, quest_sql):
        if not path.is_file():
            raise SystemExit(f"Missing {path}")

    print("Parsing quest_template_addon masks...")
    addon_masks = load_quest_addon_masks(ref_dir)

    print("Parsing creature_queststarter...")
    starter_rows_raw = extract_insert_rows(starter_sql, "creature_queststarter")
    starter_rows = [map_creature_queststarter_row(r) for r in starter_rows_raw]

    wanted_ids: set[int] = set()
    for inner in starter_rows_raw:
        wanted_ids.add(int(inner[1].strip()))

    print(f"Parsing quest_template ({len(wanted_ids)} quest ids referenced)...")
    quest_cols = extract_create_table_columns(quest_sql, "quest_template")
    col_index = {name: i for i, name in enumerate(quest_cols)}
    for required in ("ID", "QuestLevel", "LogTitle", "Flags"):
        if required not in col_index:
            raise SystemExit(
                f"quest_template in reference missing column {required!r}; "
                f"found: {quest_cols[:20]}..."
            )

    quest_rows_raw = extract_insert_rows(quest_sql, "quest_template")
    quest_rows: list[str] = []
    for row in quest_rows_raw:
        mapped = map_quest_template_row(row, col_index, wanted_ids, addon_masks)
        if mapped:
            quest_rows.append(mapped)

    header = (
        "-- Quest gossip data from firelands-cata-ref (creature_queststarter + quest_template).\n"
        "-- Used by IQuestGossipRepository / SMSG_GOSSIP_MESSAGE quest block.\n"
        "-- JDBC-safe: DELETE + batched REPLACE (re-runnable).\n"
        "-- Regenerate: python3 tools/sql/import_ref_quest_gossip.py\n"
        "-- Requires migration 36_world_quest_gossip_tables.sql (DDL).\n"
        "\n"
        "USE `firelands_world`;\n"
        "\n"
        "DELETE FROM `creature_queststarter`;\n"
        "DELETE FROM `quest_template`;\n"
        "\n"
    )

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as out:
        out.write(header)
        write_batched(
            out,
            f"REPLACE INTO `quest_template` ({QUEST_TEMPLATE_COLUMNS}) VALUES",
            quest_rows,
            400,
        )
        write_batched(
            out,
            f"REPLACE INTO `creature_queststarter` ({CREATURE_QUESTSTARTER_COLUMNS}) VALUES",
            starter_rows,
            500,
        )

    mask_out = out_path.parent / "40_world_quest_gossip_allowable_masks.sql"
    mask_updates = []
    for qid in sorted(wanted_ids):
        ac, ar = addon_masks.get(qid, (0, 0))
        mask_updates.append(
            f"UPDATE `quest_template` SET `AllowableClasses`={ac}, "
            f"`AllowableRaces`={ar} WHERE `ID`={qid};"
        )
    mask_header = (
        "-- Backfill AllowableClasses / AllowableRaces (when 38 predates mask columns).\n"
        "-- Regenerate: python3 tools/sql/import_ref_quest_gossip.py\n"
        "\nUSE `firelands_world`;\n\n"
    )
    mask_out.write_text(mask_header + "\n".join(mask_updates) + "\n", encoding="utf-8")

    mib = out_path.stat().st_size / (1024 * 1024)
    mask_kib = mask_out.stat().st_size / 1024
    print(
        f"Wrote {out_path.name}: quests={len(quest_rows)} starters={len(starter_rows)} "
        f"({mib:.2f} MiB)"
    )
    print(f"Wrote {mask_out.name}: {len(mask_updates)} UPDATEs ({mask_kib:.1f} KiB)")


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
        default=_REPO_ROOT / "sql" / "migrations" / "38_world_quest_gossip_data.sql",
        help="Output migration SQL path",
    )
    args = ap.parse_args()

    ref_db_world = args.ref / "data" / "sql" / "base" / "db_world"
    if not ref_db_world.is_dir():
        raise SystemExit(
            f"Missing ref db_world directory: {ref_db_world}\n"
            "Clone firelands-cata-ref next to this repo, then re-run."
        )

    write_quest_gossip_data_migration(ref_db_world, args.out)


if __name__ == "__main__":
    main()
