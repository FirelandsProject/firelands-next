#!/usr/bin/env python3
"""Emit migration SQL to backfill quest_template.PrevQuestId from ref addon."""

from __future__ import annotations

import re
import sys
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parents[2]
_TOOLS_SQL = Path(__file__).resolve().parent
if str(_TOOLS_SQL) not in sys.path:
    sys.path.insert(0, str(_TOOLS_SQL))

from import_ref_creature_data import extract_insert_rows  # noqa: E402


def load_prev_quest_ids(ref_dir: Path) -> dict[int, int]:
    addon_sql = ref_dir / "quest_template_addon.sql"
    if not addon_sql.is_file():
        raise SystemExit(f"Missing {addon_sql}")

    cols: list[str] = []
    in_create = False
    with addon_sql.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if re.search(r"CREATE TABLE\s+`?quest_template_addon`?\s*\(", line, re.I):
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

    prev_col = "PrevQuestID" if "PrevQuestID" in cols else "PrevQuestId"
    col_index = {name: i for i, name in enumerate(cols)}
    for required in ("ID", prev_col):
        if required not in col_index:
            raise SystemExit(f"quest_template_addon missing {required!r}")

    result: dict[int, int] = {}
    for row in extract_insert_rows(addon_sql, "quest_template_addon"):
        quest_id = int(row[col_index["ID"]].strip())
        tok = row[col_index[prev_col]].strip()
        if tok.upper() == "NULL" or tok == "0":
            continue
        result[quest_id] = int(tok)
    return result


def write_migration(out_path: Path, prev_by_quest: dict[int, int]) -> None:
    lines = [
        f"UPDATE `quest_template` SET `PrevQuestId`={prev} WHERE `ID`={qid};"
        for qid, prev in sorted(prev_by_quest.items())
    ]
    ddl_path = _REPO_ROOT / "sql" / "migrations" / "67_world_quest_template_prev_quest.sql"
    ddl = ddl_path.read_text(encoding="utf-8") if ddl_path.is_file() else ""
    header = (
        "-- PrevQuestId: DDL (67) + addon backfill. Regenerate: "
        "python3 tools/sql/import_ref_quest_prev_quest.py\n"
        f"{ddl.strip()}\n\n"
        "USE `firelands_world`;\n\n"
    )
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(header + "\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {out_path.name}: {len(lines)} UPDATEs")


def main() -> None:
    ref_db_world = _REPO_ROOT / "firelands-cata-ref" / "data" / "sql" / "base" / "db_world"
    out = _REPO_ROOT / "sql" / "migrations" / "68_world_quest_prev_quest_data.sql"
    prev_by_quest = load_prev_quest_ids(ref_db_world)
    write_migration(out, prev_by_quest)


if __name__ == "__main__":
    main()
