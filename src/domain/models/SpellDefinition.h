#pragma once

#include <shared/Common.h>

namespace Firelands {

/// Subset of client `Spell.dbc` (4.3.4) fields loaded for server-side spell logic.
/// Field indices follow SpellEntryfmt / SpellEntry in DBCStructure.h.
struct SpellDefinition {
  uint32 id = 0;
  uint32 attributes = 0;
  /// `Spell.dbc` `AttributesEx` (SPELL_ATTR_EX_* / wowdev first extended attribute dword).
  uint32 attributesEx = 0;
  /// `Spell.dbc` `AttributesEx2` (SPELL_ATTR2_* / wowdev AttributesExB).
  uint32 attributesEx2 = 0;
  uint32 castingTimeIndex = 0;
  uint32 durationIndex = 0;
  uint32 powerType = 0;
  uint32 rangeIndex = 0;
  uint32 schoolMask = 0;
  /// `Spell.dbc` → `SpellCategories.dbc` row id (CategoriesID).
  uint32 categoriesId = 0;
  /// `Spell.dbc` → `SpellCooldowns.dbc` (CooldownsID field).
  uint32 cooldownsId = 0;
  /// `Spell.dbc` field 42 (PowerDisplayID): row id in `SpellPower.dbc` for base mana.
  uint32 spellPowerId = 0;
  /// Resource1 cost from `SpellPower.dbc` via `spellPowerId` (after merge loads ids).
  uint32 manaCost = 0;
  /// First spell-hit immediate HP delta from `SpellEffect.dbc` (school damage / generic heal).
  /// Negative removes HP (damage); positive restores HP (heal). Zero = none for Phase D simplification.
  int32 immediateHealthEffectDelta = 0;
  /// Any `SpellEffect.dbc` row uses `SPELL_EFFECT_HEAL` for this spell (all indices scanned).
  bool spellEffectHasHealKind = false;
  /// Any row uses school damage, health leech, or environmental damage (polarity hint when delta is 0).
  bool spellEffectHasHarmKind = false;
};

} // namespace Firelands
