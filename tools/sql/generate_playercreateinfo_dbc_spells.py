#!/usr/bin/env python3
"""
Generate playercreateinfo_spell rows from client DBCs (SkillLineAbility,
SkillRaceClassInfo) plus playercreateinfo_skills for learn-on-skill grants.

Mirrors runtime logic previously in StarterSpellsDbc + PlayerCreateInfoService.
Emits INSERT IGNORE migration SQL so starter spells live in the world DB only.

Usage:
  python3 tools/sql/generate_playercreateinfo_dbc_spells.py
  python3 tools/sql/generate_playercreateinfo_dbc_spells.py \\
      --out sql/migrations/62_world_playercreateinfo_dbc_spells.sql
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parents[2]
_TOOLS_SQL = Path(__file__).resolve().parent
if str(_TOOLS_SQL) not in sys.path:
    sys.path.insert(0, str(_TOOLS_SQL))

from import_ref_creature_data import extract_insert_rows  # noqa: E402
from import_ref_playercreateinfo import (  # noqa: E402
    _EXCLUDED_STARTER_SKILL_IDS,
    _RIDING_SPELL_IDS,
    _WARLOCK_QUEST_SUMMON_SPELL_IDS,
    append_batched,
    map_skill_row,
)

# SkillLineAbility.dbc — niiiiiiiiiiiii (14 fields, 56 bytes @ 4.3.4.15595)
_SLA_FMT = "niiiiiiiiiiiii"
# SkillRaceClassInfo.dbc — diiiiiiii (9 fields, 36 bytes)
_SRCI_FMT = "diiiiiiii"

_CLASS_SPELL_TAB_SKILLS = frozenset(
    {
        6,
        8,
        38,
        39,
        50,
        51,
        56,
        78,
        134,
        163,
        184,
        237,
        253,
        267,
        354,
        355,
        373,
        374,
        375,
        573,
        574,
        593,
        594,
        613,
        770,
        771,
        772,
        795,
        796,
        797,
        798,
        799,
        800,
        801,
        802,
        803,
        804,
    }
)

_META_OR_INTERNAL_SKILLS = frozenset({95, 183, 777, 778, 810})
_SECONDARY_PROFESSION_SKILLS = frozenset({129, 185, 356, 762, 794})
_PRIMARY_PROFESSION_SKILLS = frozenset(
    {164, 165, 171, 182, 186, 197, 202, 333, 393, 773}
)


def player_race_mask(race_id: int) -> int:
    return 0 if race_id == 0 else 1 << (race_id - 1)


def player_class_mask(class_id: int) -> int:
    return 0 if class_id == 0 else 1 << (class_id - 1)


def mask_allows_player(mask: int, player_mask: int) -> bool:
    if mask in (0, 0xFFFFFFFF):
        return True
    return (mask & player_mask) != 0


def read_dbc_uint_rows(dbc_path: Path, fmt: str) -> list[list[int]]:
    data = dbc_path.read_bytes()
    if len(data) < 20 or data[:4] != b"WDBC":
        raise ValueError(f"Not a WDBC file: {dbc_path}")
    record_count, field_count, record_size = struct.unpack_from("<III", data, 4)
    expected_size = 4 * field_count
    if record_size != expected_size:
        raise ValueError(
            f"{dbc_path.name}: record_size={record_size}, expected {expected_size}"
        )
    offset = 20
    rows: list[list[int]] = []
    for _ in range(record_count):
        base = offset
        row = [
            struct.unpack_from("<I", data, base + i * 4)[0] for i in range(field_count)
        ]
        rows.append(row)
        offset += record_size
    return rows


def load_skill_line_categories(skill_line_dbc: Path) -> dict[int, int]:
    rows = read_dbc_uint_rows(skill_line_dbc, "niiiiii")
    return {row[0]: row[1] for row in rows if row[0] != 0}


def is_excluded_spell_grant_skill_line(skill_id: int, categories: dict[int, int]) -> bool:
    if skill_id in _META_OR_INTERNAL_SKILLS:
        return True
    if skill_id in _CLASS_SPELL_TAB_SKILLS:
        return True
    if skill_id in _SECONDARY_PROFESSION_SKILLS:
        return True
    if skill_id in _PRIMARY_PROFESSION_SKILLS:
        return True
    cat = categories.get(skill_id)
    if cat is None:
        return False
    return cat in (5, 11, 12)  # Guild, Profession, Generic


def is_allowed_starter_skill_line(skill_id: int, categories: dict[int, int]) -> bool:
    if skill_id in _SECONDARY_PROFESSION_SKILLS:
        return False
    cat = categories.get(skill_id)
    if cat is None:
        return False
    return cat in (6, 8, 10)  # Weapon, Armor, Language


def allows_starter_acquire(acquire_method: int) -> bool:
    return acquire_method in (0, 1, 2)


def finalize_candidates(
    candidates: set[int], abilities: list[tuple[int, int, int]]
) -> set[int]:
    superceded_by: dict[int, int] = {}
    for skill_line, spell_id, super_spell in abilities:
        if spell_id not in candidates:
            continue
        if super_spell != 0:
            superceded_by[spell_id] = super_spell
    remove: set[int] = set()
    for spell_id in candidates:
        newer = superceded_by.get(spell_id)
        if newer is not None and newer in candidates:
            remove.add(newer)
    return candidates - remove


def load_skill_race_class(srci_path: Path) -> list[tuple[int, int, int]]:
    rows = read_dbc_uint_rows(srci_path, _SRCI_FMT)
    out: list[tuple[int, int, int]] = []
    for row in rows:
        skill_id = row[1]
        if skill_id == 0:
            continue
        out.append((skill_id, row[2], row[3]))
    return out


def load_skill_line_abilities(sla_path: Path) -> list[dict[str, int]]:
    rows = read_dbc_uint_rows(sla_path, _SLA_FMT)
    out: list[dict[str, int]] = []
    for row in rows:
        spell_id = row[2]
        if spell_id == 0:
            continue
        out.append(
            {
                "skillLine": row[1],
                "spellId": spell_id,
                "raceMask": row[3],
                "classMask": row[4],
                "minSkillLineRank": row[7],
                "supercededBySpell": row[8],
                "acquireMethod": row[9],
            }
        )
    return out


def collect_skill_line_spells(
    race: int,
    klass: int,
    *,
    weapon_armor_language_only: bool,
    srci: list[tuple[int, int, int]],
    abilities: list[dict[str, int]],
    categories: dict[int, int],
) -> set[int]:
    if klass == 0:
        return set()
    race_mask = player_race_mask(race)
    class_mask = player_class_mask(klass)

    skill_lines: set[int] = set()
    for skill_id, src_race, src_class in srci:
        if not mask_allows_player(src_class, class_mask):
            continue
        if not mask_allows_player(src_race, race_mask):
            continue
        if is_excluded_spell_grant_skill_line(skill_id, categories):
            continue
        if weapon_armor_language_only and not is_allowed_starter_skill_line(
            skill_id, categories
        ):
            continue
        skill_lines.add(skill_id)

    candidates: set[int] = set()
    for row in abilities:
        if (
            row["skillLine"] == 183
            and row["spellId"] == 6603
            and row["minSkillLineRank"] <= 1
            and allows_starter_acquire(row["acquireMethod"])
            and mask_allows_player(row["classMask"], class_mask)
        ):
            candidates.add(row["spellId"])
            continue
        if row["skillLine"] not in skill_lines:
            continue
        if is_excluded_spell_grant_skill_line(row["skillLine"], categories):
            continue
        if row["minSkillLineRank"] > 1:
            continue
        if not allows_starter_acquire(row["acquireMethod"]):
            continue
        if not mask_allows_player(row["classMask"], class_mask):
            continue
        rm = row["raceMask"]
        if rm not in (0, 0xFFFFFFFF) and not mask_allows_player(rm, race_mask):
            continue
        if row["spellId"] in _RIDING_SPELL_IDS:
            continue
        if row["spellId"] in _WARLOCK_QUEST_SUMMON_SPELL_IDS:
            continue
        candidates.add(row["spellId"])

    super_rows = [
        (r["skillLine"], r["spellId"], r["supercededBySpell"]) for r in abilities
    ]
    return finalize_candidates(candidates, super_rows)


def get_racial_spells(
    race: int,
    klass: int,
    abilities: list[dict[str, int]],
) -> set[int]:
    if race == 0 or klass == 0:
        return set()
    race_mask = player_race_mask(race)
    class_mask = player_class_mask(klass)
    candidates: set[int] = set()
    for row in abilities:
        rm = row["raceMask"]
        if rm in (0, 0xFFFFFFFF):
            continue
        if not mask_allows_player(rm, race_mask):
            continue
        if not mask_allows_player(row["classMask"], class_mask):
            continue
        if row["minSkillLineRank"] > 1:
            continue
        if not allows_starter_acquire(row["acquireMethod"]):
            continue
        if row["spellId"] in _RIDING_SPELL_IDS:
            continue
        candidates.add(row["spellId"])
    super_rows = [
        (r["skillLine"], r["spellId"], r["supercededBySpell"]) for r in abilities
    ]
    return finalize_candidates(candidates, super_rows)


def get_learn_on_skill_spells(
    race: int,
    klass: int,
    class_tab_skills: set[int],
    abilities: list[dict[str, int]],
) -> set[int]:
    if klass == 0 or not class_tab_skills:
        return set()
    race_mask = player_race_mask(race)
    class_mask = player_class_mask(klass)
    candidates: set[int] = set()
    for row in abilities:
        if row["skillLine"] not in class_tab_skills:
            continue
        if row["acquireMethod"] != 2:
            continue
        if row["minSkillLineRank"] > 1:
            continue
        if not mask_allows_player(row["classMask"], class_mask):
            continue
        rm = row["raceMask"]
        if rm not in (0, 0xFFFFFFFF) and not mask_allows_player(rm, race_mask):
            continue
        if row["spellId"] in _RIDING_SPELL_IDS:
            continue
        if row["spellId"] in _WARLOCK_QUEST_SUMMON_SPELL_IDS:
            continue
        candidates.add(row["spellId"])
    super_rows = [
        (r["skillLine"], r["spellId"], r["supercededBySpell"]) for r in abilities
    ]
    return finalize_candidates(candidates, super_rows)


def load_playercreateinfo_pairs(ref_dir: Path) -> list[tuple[int, int]]:
    pairs: list[tuple[int, int]] = []
    for fields in extract_insert_rows(ref_dir / "playercreateinfo.sql", "playercreateinfo"):
        if len(fields) < 2:
            continue
        pairs.append((int(fields[0]), int(fields[1])))
    return pairs


def load_class_tab_skills_by_combo(
    ref_dir: Path,
) -> dict[tuple[int, int], set[int]]:
    by_combo: dict[tuple[int, int], set[int]] = {}
    pairs = load_playercreateinfo_pairs(ref_dir)
    skill_rows: list[tuple[int, int, int, int]] = []
    for fields in extract_insert_rows(
        ref_dir / "playercreateinfo_skills.sql", "playercreateinfo_skills"
    ):
        mapped = map_skill_row(fields)
        if mapped is None:
            continue
        rm, cm, sid, rank = [int(x) for x in mapped.split(",")]
        skill_rows.append((rm, cm, sid, rank))

    for race, klass in pairs:
        race_mask = player_race_mask(race)
        class_mask = player_class_mask(klass)
        tabs: set[int] = set()
        for rm, cm, sid, _rank in skill_rows:
            if sid in _EXCLUDED_STARTER_SKILL_IDS:
                continue
            if sid not in _CLASS_SPELL_TAB_SKILLS:
                continue
            if cm != 0 and (cm & class_mask) == 0:
                continue
            if rm != 0 and (rm & race_mask) == 0:
                continue
            tabs.add(sid)
        by_combo[(race, klass)] = tabs
    return by_combo


def consolidate_rows(raw_rows: set[tuple[int, int, int]]) -> list[tuple[int, int, int]]:
    """Prefer (0, classMask) when every race/class combo for that class shares the spell."""
    by_spell_class: dict[tuple[int, int], set[int]] = {}
    for race_mask, class_mask, spell_id in raw_rows:
        by_spell_class.setdefault((spell_id, class_mask), set()).add(race_mask)

    consolidated: set[tuple[int, int, int]] = set()
    for (spell_id, class_mask), race_masks in by_spell_class.items():
        if race_masks == {0}:
            consolidated.add((0, class_mask, spell_id))
            continue
        if 0 in race_masks and len(race_masks) == 1:
            consolidated.add((0, class_mask, spell_id))
            continue
        for rm in race_masks:
            consolidated.add((rm, class_mask, spell_id))
    return sorted(consolidated)


def generate_rows(
    *,
    dbc_dir: Path,
    ref_dir: Path,
) -> list[tuple[int, int, int]]:
    categories = load_skill_line_categories(dbc_dir / "SkillLine.dbc")
    srci = load_skill_race_class(dbc_dir / "SkillRaceClassInfo.dbc")
    abilities = load_skill_line_abilities(dbc_dir / "SkillLineAbility.dbc")
    class_tabs = load_class_tab_skills_by_combo(ref_dir)

    raw: set[tuple[int, int, int]] = set()
    for race, klass in load_playercreateinfo_pairs(ref_dir):
        race_mask = player_race_mask(race)
        class_mask = player_class_mask(klass)
        spells: set[int] = set()
        spells |= collect_skill_line_spells(
            race,
            klass,
            weapon_armor_language_only=True,
            srci=srci,
            abilities=abilities,
            categories=categories,
        )
        spells |= get_racial_spells(race, klass, abilities)
        spells |= get_learn_on_skill_spells(
            race, klass, class_tabs.get((race, klass), set()), abilities
        )
        for spell_id in spells:
            raw.add((race_mask, class_mask, spell_id))
    return consolidate_rows(raw)


def write_migration(rows: list[tuple[int, int, int]], out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    lines: list[str] = [
        "-- Weapon/armor/language, racial, and learn-on-skill starter spells from client DBCs.",
        "-- Generated by tools/sql/generate_playercreateinfo_dbc_spells.py",
        "-- Replaces runtime StarterSpellsDbc merge in PlayerCreateInfoService.",
        "USE `firelands_world`;",
        "",
    ]
    sql_rows = [f"({rm},{cm},{sid})" for rm, cm, sid in rows]
    append_batched(
        lines,
        "INSERT IGNORE INTO `playercreateinfo_spell` (`raceMask`, `classMask`, `spellId`) VALUES",
        sql_rows,
        batch_size=120,
    )
    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--dbc",
        type=Path,
        default=_REPO_ROOT / "data" / "dbc",
        help="Client DBC directory (SkillLine*.dbc, SkillRaceClassInfo.dbc)",
    )
    parser.add_argument(
        "--ref",
        type=Path,
        default=_REPO_ROOT / "firelands-cata-ref" / "data" / "sql" / "base" / "db_world",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=_REPO_ROOT / "sql" / "migrations" / "62_world_playercreateinfo_dbc_spells.sql",
    )
    args = parser.parse_args()

    for name in ("SkillLine.dbc", "SkillLineAbility.dbc", "SkillRaceClassInfo.dbc"):
        path = args.dbc / name
        if not path.is_file():
            raise SystemExit(f"Missing {path} — extract client DBCs to data/dbc")

    rows = generate_rows(dbc_dir=args.dbc, ref_dir=args.ref)
    write_migration(rows, args.out)
    print(f"Wrote {args.out} ({len(rows)} playercreateinfo_spell rows)")


if __name__ == "__main__":
    main()
