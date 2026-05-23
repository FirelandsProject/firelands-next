#pragma once

#include <shared/Common.h>

namespace Firelands {

/// Client DBC lookups tied to `Spell.dbc` indices (`CastingTimeIndex`, `RangeIndex`).
/// Implementations are read-only after load; hot path must be O(1) without allocations.
class ISpellCastTables {
public:
  virtual ~ISpellCastTables() = default;

  /// Milliseconds from `SpellCastTimes.dbc` for `castingTimeIndex`; 0 if unknown or index 0.
  virtual uint32 GetCastTimeMs(uint32 castingTimeIndex) const = 0;

  /// SpellRangeEntry: fields 1–4 are `RangeMin[0]`, `RangeMin[1]`, `RangeMax[0]`,
  /// `RangeMax[1]` (hostile vs friendly pairs). Returns 0 if unknown / index 0.
  virtual float GetSpellRangeMinYards(uint32 rangeIndex, bool friendlyTarget) const = 0;

  virtual float GetSpellRangeMaxYards(uint32 rangeIndex, bool friendlyTarget) const = 0;

  /// `SpellCooldowns.dbc` row for `Spell.dbc` `CooldownsID` (0 = leave outputs zeroed).
  virtual void GetCooldownTiming(uint32 cooldownsId, uint32 *categoryRecoveryMs,
                                 uint32 *recoveryMs, uint32 *startRecoveryMs) const = 0;

    /// Resolved POWER1 cost from `SpellPower.dbc` at cast time (flat, per-level, % of base POWER1).
    /// `spellPowerType` is the caster's primary power; `power1PoolForPercent` is base mana for % rows.
    virtual uint32 ResolveSpellPowerCost(uint32 spellPowerId, uint32 spellPowerType,
                                                                          uint8 casterLevel, uint8 spellLevel,
                                                                          uint32 power1PoolForPercent) const = 0;

  /// `SpellCategories.dbc` `Category` field for row id `categoriesId` from `Spell.dbc`
  /// (`CategoriesID`). Used with `SpellCooldowns.dbc` `CategoryRecoveryTime` for category
  /// cooldown. Returns 0 if unknown or `categoriesId` is 0.
  virtual uint32 GetSpellCategoryGroupForCategoriesId(uint32 categoriesId) const = 0;

  /// `SpellDuration.dbc` duration for `durationIndex` (base + per-level, capped by max); 0 if unknown.
  virtual uint32 GetDurationMs(uint32 durationIndex, uint8 casterLevel) const = 0;
};

} // namespace Firelands
