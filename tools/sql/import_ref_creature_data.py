#!/usr/bin/env python3
"""
Convert Firelands Cata reference `creature.sql` / `creature_template.sql` dumps
(tab-separated mysqldump multi-row INSERTs) into Firelands Next world schema.

Reads Trinity-compatible column order from the reference CREATE TABLE and emits:
  USE `firelands_world`;
  DELETE FROM `creature`;
  Batched REPLACE INTO `creature_template` (...)
  Batched INSERT INTO `creature` (...)

Default paths assume `firelands-cata-ref` next to this repo (gitignored clone).

Usage:
  python3 tools/sql/import_ref_creature_data.py
  python3 tools/sql/import_ref_creature_data.py --ref /path/to/firelands-cata-ref \\
      --out sql/bundled/creature_ref_import.sql
"""

from __future__ import annotations

import argparse
from pathlib import Path


def split_top_level_tuple_inners(values_blob: str) -> list[str]:
    """Parse mysqldump VALUES blob: (row),(row)... → list of inner strings per row."""
    s = values_blob.strip()
    if s.endswith(";"):
        s = s[:-1].strip()
    out: list[str] = []
    i = 0
    n = len(s)
    while i < n:
        while i < n and s[i] in " \t\n\r,":
            i += 1
        if i >= n:
            break
        if s[i] != "(":
            i += 1
            continue
        depth = 0
        start = i
        j = i
        while j < n:
            c = s[j]
            if c == "'":
                j += 1
                while j < n:
                    if s[j] == "\\":
                        j += 2
                        continue
                    if s[j] == "'":
                        j += 1
                        break
                    j += 1
                continue
            if c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
                if depth == 0:
                    out.append(s[start + 1 : j])
                    j += 1
                    i = j
                    break
            j += 1
        else:
            break
    return out


def split_sql_fields(inner: str) -> list[str]:
    parts: list[str] = []
    cur: list[str] = []
    i = 0
    in_str = False
    while i < len(inner):
        c = inner[i]
        if in_str:
            cur.append(c)
            if c == "\\" and i + 1 < len(inner):
                cur.append(inner[i + 1])
                i += 2
                continue
            if c == "'":
                in_str = False
            i += 1
            continue
        if c == "'":
            in_str = True
            cur.append(c)
            i += 1
            continue
        if c == ",":
            parts.append("".join(cur).strip())
            cur = []
            i += 1
            continue
        cur.append(c)
        i += 1
    parts.append("".join(cur).strip())
    return parts


def strip_sql_string(tok: str) -> str:
    tok = tok.strip()
    if len(tok) >= 2 and tok[0] == "'" and tok[-1] == "'":
        body = tok[1:-1]
        return (
            body.replace("\\\\", "\\")
            .replace("\\'", "'")
            .replace('\\"', '"')
            .replace("''", "'")
        )
    return tok


def sql_escape_literal(text: str) -> str:
    return text.replace("\\", "\\\\").replace("'", "''")


def sql_string(text: str, max_len: int | None = None) -> str:
    if max_len is not None:
        text = text[:max_len]
    return "N'" + sql_escape_literal(text) + "'"


def extract_insert_rows(sql_path: Path, table: str) -> list[list[str]]:
    prefix_needle = f"INSERT INTO `{table}` VALUES"
    rows: list[list[str]] = []
    with sql_path.open("r", encoding="utf8", errors="replace") as handle:
        for line in handle:
            line_stripped = line.strip()
            if not line_stripped.startswith("INSERT INTO"):
                continue
            if f"`{table}`" not in line_stripped:
                continue
            vi = line_stripped.find("VALUES")
            if vi < 0:
                continue
            blob = line_stripped[vi + 6 :].strip()
            for inner in split_top_level_tuple_inners(blob):
                fields = split_sql_fields(inner)
                rows.append(fields)
    return rows


def map_creature_template_row(f: list[str]) -> str:
    """Reference creature_template column order (80 fields). → Firelands Next."""
    if len(f) < 80:
        raise ValueError(f"creature_template row expected 80 fields, got {len(f)}")
    entry = f[0]
    kill1, kill2 = f[4], f[5]

    def text_or_empty(idx: int) -> str:
        tok = f[idx]
        if tok.upper() == "NULL":
            return ""
        return strip_sql_string(tok) if tok.startswith("'") else tok

    name = text_or_empty(10)
    female = text_or_empty(11)

    # Firelands Next may retain `subname NOT NULL` from migration 23; Trinity allows NULL.
    sub_tok = f[12]
    if sub_tok.upper() == "NULL":
        sub_sql = "N''"
    else:
        sub_txt = strip_sql_string(sub_tok) if sub_tok.startswith("'") else sub_tok
        sub_sql = sql_string(sub_txt)

    icon_tok = f[13]
    if icon_tok.upper() == "NULL":
        icon_sql = "NULL"
    else:
        icon_raw = strip_sql_string(icon_tok) if icon_tok.startswith("'") else icon_tok
        icon_sql = sql_string(icon_raw[:64]) if icon_raw else "NULL"

    gossip_menu_id = f[14].strip()
    modelid1, modelid2, modelid3, modelid4 = f[6], f[7], f[8], f[9]
    minlevel, maxlevel = f[15], f[16]
    faction = f[19]
    npcflag = f[20]
    speed_walk, speed_run, scale = f[21], f[22], f[23]
    classification = f[24]
    dmgschool = f[25]
    bat, rat = f[26], f[27]
    bv, rv = f[28], f[29]
    unit_class = f[30]
    unit_flags = f[31]
    unit_flags2 = f[32]
    unit_flags3 = f[33]
    family = f[34]
    trainer_class = f[36]
    ctype = f[38]
    vehicle = f[59]
    ainame_tok = f[62]
    if ainame_tok.upper() == "NULL":
        ainame = ""
    else:
        ainame_raw = strip_sql_string(ainame_tok) if ainame_tok.startswith("'") else ainame_tok
        ainame = ainame_raw[:64]
    movement_type = f[63]
    exp_mod = f[71]
    racial_leader = f[72]
    movement_id = f[73]
    regen_health = f[74]
    flags_extra = f[77]
    script_raw = f[78]
    if script_raw.upper() == "NULL":
        script = ""
    else:
        script = strip_sql_string(script_raw) if script_raw.startswith("'") else script_raw
        script = script[:64]
    verified = f[79]

    vals = [
        entry,
        kill1,
        kill2,
        modelid1,
        modelid2,
        modelid3,
        modelid4,
        sql_string(name),
        sql_string(female),
        sub_sql,
        "N''",
        icon_sql,
        gossip_menu_id,
        "0",
        "0",
        faction,
        npcflag,
        speed_walk,
        speed_run,
        scale,
        classification,
        dmgschool,
        bat,
        rat,
        bv,
        rv,
        unit_class,
        unit_flags,
        unit_flags2,
        unit_flags3,
        family,
        trainer_class,
        ctype,
        vehicle,
        sql_string(ainame) if ainame else "N''",
        movement_type,
        exp_mod,
        racial_leader,
        movement_id,
        "0",
        "0",
        regen_health,
        "0",
        flags_extra,
        sql_string(script) if script else "N''",
        "NULL",
        verified,
        minlevel,
        maxlevel,
    ]
    return "(" + ",".join(vals) + ")"


def map_creature_row(f: list[str]) -> str:
    """Reference creature (28 fields) → Firelands creature."""
    if len(f) < 28:
        raise ValueError(f"creature row expected 28 fields, got {len(f)}")
    guid = f[0]
    cid = f[1]
    map_id = f[2]
    zone_id = f[3]
    area_id = f[4]
    spawn_mask = f[5]
    phase_use = f[6]
    phase_id = f[8]
    phase_group = f[9]
    terrain_swap = f[10]
    modelid = f[11]
    equip = f[12]
    px, py, pz = f[13], f[14], f[15]
    ori = f[16]
    spawn_time = f[17]
    wander = f[18]
    waypoint = f[19]
    curhealth = f[20]
    _mana = f[21]
    movement_type = f[22]
    npcflag = f[23]
    unit_flags = f[24]
    _dynamic = f[25]
    script_raw = f[26]
    verified = f[27]

    try:
        hp = int(curhealth.strip())
    except ValueError:
        hp = 100
    if hp <= 0:
        hp_pct = "100"
    elif hp <= 100:
        hp_pct = str(hp)
    else:
        hp_pct = "100"

    if script_raw.upper() == "NULL":
        script = "N''"
    else:
        body = strip_sql_string(script_raw) if script_raw.startswith("'") else script_raw
        script = sql_string(body[:64])

    spawn_diff = sql_string(spawn_mask.strip())

    vals = [
        guid,
        cid,
        map_id,
        zone_id,
        area_id,
        spawn_diff,
        phase_use,
        phase_id,
        phase_group,
        terrain_swap,
        modelid,
        equip,
        px,
        py,
        pz,
        ori,
        spawn_time,
        wander,
        waypoint,
        hp_pct,
        movement_type,
        npcflag,
        unit_flags,
        "NULL",
        "NULL",
        script,
        "NULL",
        verified,
    ]
    return "(" + ",".join(vals) + ")"


TEMPLATE_COLUMNS = (
    "`entry`, `KillCredit1`, `KillCredit2`, `modelid1`, `modelid2`, `modelid3`, `modelid4`, "
    "`name`, `femaleName`, `subname`, `TitleAlt`, "
    "`IconName`, `gossip_menu_id`, `RequiredExpansion`, `VignetteID`, `faction`, `npcflag`, `speed_walk`, "
    "`speed_run`, `scale`, `Classification`, `dmgschool`, `BaseAttackTime`, `RangeAttackTime`, "
    "`BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `unit_flags3`, "
    "`family`, `trainer_class`, `type`, `VehicleId`, `AIName`, `MovementType`, "
    "`ExperienceModifier`, `RacialLeader`, `movementId`, `WidgetSetID`, `WidgetSetUnitConditionID`, "
    "`RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `StringId`, "
    "`VerifiedBuild`, `minlevel`, `maxlevel`"
)

CREATURE_COLUMNS = (
    "`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `phaseUseFlags`, "
    "`PhaseId`, `PhaseGroup`, `terrainSwapMap`, `modelid`, `equipment_id`, "
    "`position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, "
    "`wander_distance`, `currentwaypoint`, `curHealthPct`, `MovementType`, `npcflag`, "
    "`unit_flags`, `unit_flags2`, `unit_flags3`, `ScriptName`, `StringId`, `VerifiedBuild`"
)


def build_template_model_map(template_sql: Path) -> dict[str, tuple[str, str, str, str]]:
    """entry -> (modelid1..4) from Trinity `creature_template.sql` dump."""
    rows = extract_insert_rows(template_sql, "creature_template")
    out: dict[str, tuple[str, str, str, str]] = {}
    for r in rows:
        if len(r) < 80:
            continue
        entry = r[0].strip()
        out[entry] = (r[6].strip(), r[7].strip(), r[8].strip(), r[9].strip())
    return out


def _migration_row_is_creature_template_tuple(fields: list[str]) -> bool:
    if len(fields) < 4:
        return False
    fn = fields[3].strip()
    return fn.startswith("N'") or fn.startswith("'")


def _peel_outer_tuple_sql_row(line_stripped: str) -> tuple[str, str] | None:
    """Return (inner_csv, closing_suffix) for `(inner),` / `(inner);` / `(inner),` variants."""
    s = line_stripped.strip()
    if not s.startswith("("):
        return None
    # Must check longest suffix first: `(row)),` wrongly matches `endswith("),")`.
    if s.endswith(")),"):
        return s[1:-3], "),"
    if s.endswith("),"):
        return s[1:-2], "),"
    if s.endswith(");"):
        return s[1:-2], ");"
    if s.endswith(","):
        return s[1:-1], ","
    return None


def write_gossip_menu_id_migration(
    template_sql: Path,
    out_path: Path,
    *,
    batch_size: int = 250,
) -> None:
    """Emit JDBC-safe migration: ADD `gossip_menu_id` + CASE batched backfill from ref dump."""
    rows = extract_insert_rows(template_sql, "creature_template")
    pairs: list[tuple[str, str]] = []
    for r in rows:
        if len(r) < 80:
            continue
        entry = r[0].strip()
        gid = r[14].strip()
        try:
            if int(gid) <= 0:
                continue
        except ValueError:
            continue
        pairs.append((entry, gid))

    header = (
        "-- Links creature_template to gossip_menu (Trinity `gossip_menu_id` → MenuID).\n"
        "-- Data backfill from firelands-cata-ref creature_template.sql.\n"
        "-- JDBC-safe conditional DDL + batched UPDATE … CASE.\n"
        "-- Regenerate: python3 tools/sql/import_ref_creature_data.py --write-gossip-menu-id-migration\n"
        "-- Run after migration 30 (creature ref import). Before gossip_menu INSERTs.\n"
        "\n"
        "USE `firelands_world`;\n"
        "\n"
        "SET @exist_gossip_menu_id :=\n"
        "  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS\n"
        "   WHERE TABLE_SCHEMA = DATABASE()\n"
        "     AND TABLE_NAME = 'creature_template'\n"
        "     AND COLUMN_NAME = 'gossip_menu_id');\n"
        "\n"
        "SET @fl_sql := IF(@exist_gossip_menu_id = 0,\n"
        "  'ALTER TABLE `creature_template`\n"
        "     ADD COLUMN `gossip_menu_id` int unsigned NOT NULL DEFAULT ''0''\n"
        "     COMMENT ''Links to gossip_menu.MenuID (Trinity cata)''\n"
        "     AFTER `IconName`',\n"
        "  'SELECT 1');\n"
        "\n"
        "PREPARE _fl_m31_p FROM @fl_sql;\n"
        "EXECUTE _fl_m31_p;\n"
        "DEALLOCATE PREPARE _fl_m31_p;\n"
        "\n"
    )

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as out:
        out.write(header)
        for i in range(0, len(pairs), batch_size):
            chunk = pairs[i : i + batch_size]
            entries = ", ".join(p[0] for p in chunk)
            out.write("UPDATE `creature_template` SET `gossip_menu_id` = CASE `entry`\n")
            for entry, gid in chunk:
                out.write(f"  WHEN {entry} THEN {gid}\n")
            out.write("  ELSE `gossip_menu_id` END\n")
            out.write(f"WHERE `entry` IN ({entries});\n\n")

    print(
        f"Wrote {out_path.name}: {len(pairs)} templates with gossip_menu_id > 0 "
        f"({(len(pairs) + batch_size - 1) // batch_size} UPDATE batch(es))"
    )


def patch_migration_modelids(template_sql: Path, migration_path: Path, out_path: Path) -> None:
    """Rewrite legacy ref-import SQL (no model columns) using Trinity template modelids."""
    model_map = build_template_model_map(template_sql)
    old_frag = "`KillCredit2`, `name`"
    new_frag = (
        "`KillCredit2`, `modelid1`, `modelid2`, `modelid3`, `modelid4`, `name`"
    )

    out_path.parent.mkdir(parents=True, exist_ok=True)
    tmp_path = out_path.with_suffix(out_path.suffix + ".tmp")
    stats_templates = 0

    with migration_path.open("r", encoding="utf8", errors="replace") as inp, tmp_path.open(
        "w", encoding="utf8"
    ) as outp:
        for line in inp:
            if "REPLACE INTO `creature_template`" in line and old_frag in line:
                line = line.replace(old_frag, new_frag, 1)

            peeled = _peel_outer_tuple_sql_row(line)
            if peeled is None:
                outp.write(line)
                continue

            inner, tail = peeled
            fields = split_sql_fields(inner)
            if not _migration_row_is_creature_template_tuple(fields):
                outp.write(line)
                continue
            # Already patched (four numeric model columns before localized name)
            if fields[3].strip().isdigit():
                outp.write(line)
                continue

            entry = fields[0].strip()
            m = model_map.get(entry, ("0", "0", "0", "0"))
            new_fields = fields[:3] + list(m) + fields[3:]
            outp.write("(" + ",".join(new_fields) + ")" + tail + "\n")
            stats_templates += 1

    tmp_path.replace(out_path)
    print(
        f"Patched creature_template tuples: {stats_templates}; wrote {out_path.name}"
    )


def write_batched(
    out,
    stmt_prefix: str,
    rows: list[str],
    batch_size: int,
) -> None:
    for i in range(0, len(rows), batch_size):
        chunk = rows[i : i + batch_size]
        out.write(f"{stmt_prefix}\n")
        out.write(",\n".join(chunk))
        out.write(";\n")


def main() -> None:
    root = Path(__file__).resolve().parents[2]
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--ref",
        type=Path,
        default=root / "firelands-cata-ref",
        help="Path to firelands-cata-ref checkout",
    )
    ap.add_argument(
        "--out",
        type=Path,
        default=root / "sql" / "bundled" / "creature_ref_import.sql",
        help="Output migration SQL path (full export or patch target)",
    )
    ap.add_argument(
        "--patch-migration",
        action="store_true",
        help="Inject modelid1..4 into an existing ref-import file using `--ref` creature_template.sql (does not rebuild from scratch)",
    )
    ap.add_argument(
        "--write-gossip-menu-id-migration",
        action="store_true",
        help="Write sql/migrations/31_world_creature_template_gossip_menu_id.sql from ref gossip_menu_id values",
    )
    ap.add_argument("--template-batch", type=int, default=120)
    ap.add_argument("--creature-batch", type=int, default=200)
    args = ap.parse_args()

    creature_sql = args.ref / "data" / "sql" / "base" / "db_world" / "creature.sql"
    template_sql = args.ref / "data" / "sql" / "base" / "db_world" / "creature_template.sql"
    if not template_sql.is_file():
        raise SystemExit(f"Missing {template_sql}")

    if args.patch_migration:
        if not args.out.is_file():
            raise SystemExit(f"--patch-migration: file not found: {args.out}")
        print(f"Patching model columns using {template_sql.name} ...")
        patch_migration_modelids(template_sql, args.out, args.out)
        return

    if args.write_gossip_menu_id_migration:
        migration_out = root / "sql" / "migrations" / "31_world_creature_template_gossip_menu_id.sql"
        print(f"Generating gossip_menu_id migration from {template_sql.name} ...")
        write_gossip_menu_id_migration(template_sql, migration_out)
        return

    if not creature_sql.is_file():
        raise SystemExit(f"Missing {creature_sql}")

    print("Parsing creature_template...")
    t_rows = extract_insert_rows(template_sql, "creature_template")
    print(f"  {len(t_rows)} templates")
    mapped_templates = [map_creature_template_row(r) for r in t_rows]

    print("Parsing creature spawns...")
    c_rows = extract_insert_rows(creature_sql, "creature")
    print(f"  {len(c_rows)} spawns")
    mapped_creatures = [map_creature_row(r) for r in c_rows]

    args.out.parent.mkdir(parents=True, exist_ok=True)
    header = (
        "-- Imported from firelands-cata-ref data/sql/base/db_world/\n"
        "-- creature.sql + creature_template.sql (Trinity-compatible layout).\n"
        "-- JDBC-safe: short batched statements for MariaDB max_allowed_packet.\n"
        "-- Regenerate: python3 tools/sql/import_ref_creature_data.py\n"
        "-- File must sort AFTER `29_world_creature_template_modelids.sql` (use `30_` prefix).\n"
        "\n"
        "USE `firelands_world`;\n"
        "\n"
        "DELETE FROM `creature`;\n"
    )

    with args.out.open("w", encoding="utf8") as out:
        out.write(header)
        write_batched(
            out,
            f"REPLACE INTO `creature_template` ({TEMPLATE_COLUMNS}) VALUES",
            mapped_templates,
            args.template_batch,
        )
        write_batched(
            out,
            f"INSERT INTO `creature` ({CREATURE_COLUMNS}) VALUES",
            mapped_creatures,
            args.creature_batch,
        )

    print(f"Wrote {args.out} ({args.out.stat().st_size // (1024 * 1024)} MiB)")


if __name__ == "__main__":
    main()
