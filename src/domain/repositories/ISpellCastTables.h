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

  /// `max(RangeMax[0], RangeMax[1])` yards from `SpellRange.dbc`; 0 if unknown / index 0.
  /// Callers treat 0 as "not loaded or no limit yet" until range validation exists.
  virtual float GetHostileRangeMaxYards(uint32 rangeIndex) const = 0;
};

} // namespace Firelands
