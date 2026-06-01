#pragma once

#include <shared/Common.h>
#include <shared/game/SpellAttributes.h>
#include <shared/game/StarterSpellFilters.h>

#include <vector>

namespace Firelands {

/// One `SpellEffect.dbc` apply-aura row for a spell (passive login, stats, wire).
struct SpellAuraEffectRow {
  uint8 effectIndex = 0;
  uint32 auraType = 0;
  int32 basePoints = 0;
  int32 dieSides = 0;
  float realPointsPerLevel = 0.f;
  uint32 periodMs = 0;
  int32 periodicHealthDeltaPerTick = 0;
  /// `EffectMiscValue` — stat index, rating id, resistance school, etc.
  int32 miscValue = 0;
  uint32 miscValueB = 0;
};

/// Non-aura `SpellEffect.dbc` row (energize, damage, heal, …).
struct SpellEffectRow {
  uint8 effectIndex = 0;
  uint32 effectKind = 0;
  int32 basePoints = 0;
  int32 dieSides = 0;
  float realPointsPerLevel = 0.f;
  int32 miscValue = 0;
  uint32 miscValueB = 0;
};

/// Subset of client `Spell.dbc` (4.3.4) fields loaded for server-side spell logic.
/// Field indices follow SpellEntryfmt / SpellEntry in DBCStructure.h.
struct SpellDefinition {
  uint32 id = 0;
  uint32 attributes = 0;
  /// `Spell.dbc` `AttributesEx` (SPELL_ATTR_EX_* / wowdev first extended attribute dword).
  uint32 attributesEx = 0;
  /// `Spell.dbc` `AttributesEx2` (SPELL_ATTR_EX2_* / wowdev AttributesExB).
  uint32 attributesEx2 = 0;
  /// `Spell.dbc` `AttributesEx8` — e.g. `SPELL_ATTR8_AURA_SEND_AMOUNT`.
  uint32 attributesEx8 = 0;
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
  /// `Spell.dbc` `LevelsID` → `SpellLevels.dbc` row (0 = no level gate).
  uint32 levelsId = 0;
  /// Required character level from `SpellLevels.dbc` (`SpellLevel`); 0 = always allowed.
  uint8 requiredLevel = 0;
    /// Legacy flat cost slot (unused at cast; cost is resolved from `spellPowerId` via
    /// `ISpellCastTables::ResolveSpellPowerCost` using caster level and max POWER1).
  uint32 manaCost = 0;
  /// `Spell.dbc` `SpellVisualID[0]` / `[1]` (4.3.4 fields 19 / 23) → `SpellVisual.dbc`.
  uint32 spellVisualId0 = 0;
  uint32 spellVisualId1 = 0;
  /// First spell-hit immediate HP delta from `SpellEffect.dbc` (school damage / generic heal).
  /// Negative removes HP (damage); positive restores HP (heal). Zero = none for Phase D simplification.
  int32 immediateHealthEffectDelta = 0;
  /// Any `SpellEffect.dbc` row uses `SPELL_EFFECT_HEAL` for this spell (all indices scanned).
  bool spellEffectHasHealKind = false;
  /// Any row uses school damage, health leech, or environmental damage (polarity hint when delta is 0).
  bool spellEffectHasHarmKind = false;

  /// True if any SpellEffect.dbc row has effect = SPELL_EFFECT_APPLY_AURA (6).
  bool hasAuraEffect = false;
  /// Aura effect subtype when hasAuraEffect (e.g., SPELL_AURA_MOD_STAT, SPELL_AURA_PERIODIC_HEAL).
  uint32 auraEffectType = 0;
  /// `SpellEffect.dbc` `EffectIndex` (0–2) for the chosen apply-aura row.
  uint8 auraEffectIndex = 0;
  /// Bits 0–2 set for each `SpellEffect.dbc` apply-aura row on this spell (wire `AFLAG_EFF_INDEX_*`).
  uint8 auraActiveEffectMask = 0;
  /// Base points from SpellEffect.dbc for the aura effect (used for aura magnitude).
  int32 auraBasePoints = 0;
  /// DieSides from SpellEffect.dbc for the aura effect (used for aura magnitude range).
  int32 auraDieSides = 0;
  /// `SpellEffect.dbc` `EffectRealPointsPerLevel` (field 6) for the chosen apply-aura row.
  float auraRealPointsPerLevel = 0.f;
  /// Duration index from Spell.dbc - used to determine aura duration.
  uint32 auraDurationIndex = 0;
  /// `SpellEffect.dbc` `EffectAuraPeriod` (ms) when the aura effect is periodic.
  uint32 auraPeriodicPeriodMs = 0;
  /// Signed HP change per periodic tick (negative = damage).
  int32 auraPeriodicHealthDeltaPerTick = 0;

  /// Any `SpellEffect.dbc` apply-aura row uses a mount/vehicle aura (not shapeshift).
  bool hasMountOrVehicleAura = false;
  /// Any `SpellEffect.dbc` row uses `SPELL_EFFECT_SKILL` (118) — profession trainers.
  bool grantsSkillLine = false;

  /// All apply-aura rows from `SpellEffect.dbc` (login passives, stat aggregation).
  std::vector<SpellAuraEffectRow> auraEffects;
  /// All effect rows (cast resolution: energize, extra heals, …).
  std::vector<SpellEffectRow> effectRows;

  bool isPassiveSpell() const { return (attributes & SpellAttr0::kPassive) != 0u; }

  /// On-use racials (Blood Fury, Berserking, Stoneform, …): `PASSIVE` in spellbook with a
  /// `SpellCooldowns` row. Always-on racials still use `DurationIndex` for infinite auras.
  bool isActivatablePassiveSpell() const {
    return isPassiveSpell() && cooldownsId != 0u;
  }

  /// Always-on passives applied at login (Hardiness, resist racials, Human Spirit, …).
  bool isPermanentLoginPassiveSpell() const {
    return isPassiveSpell() && cooldownsId == 0u;
  }

  /// Infinite login auras, including racials without `SPELL_ATTR0_PASSIVE` (Da Voodoo Shuffle).
  bool isAlwaysOnLoginPassiveSpell() const {
    if (cooldownsId != 0u || isActivatablePassiveSpell())
      return false;
    if (isPermanentLoginPassiveSpell())
      return true;
    if (durationIndex != 0u || auraEffects.empty())
      return false;
    for (SpellAuraEffectRow const &row : auraEffects) {
      if (!IsAlwaysOnLoginAuraType(row.auraType))
        return false;
    }
    return true;
  }

  bool HasLoginPassiveAura() const {
    return isAlwaysOnLoginPassiveSpell() &&
           (!auraEffects.empty() || hasAuraEffect);
  }

  bool sendsAuraEffectAmountOnWire() const {
    return (attributesEx8 & SpellAttr8::kAuraSendAmount) != 0u;
  }

  /// False when `SPELL_ATTR0_CANT_CANCEL` is set (right-click buff dismiss blocked).
  bool playerCanCancelAuraByClient() const {
    return (attributes & SpellAttr0::kCantCancel) == 0u;
  }
};

} // namespace Firelands
