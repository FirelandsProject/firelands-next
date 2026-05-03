#pragma once

#include <shared/Common.h>

namespace Firelands {

/// Subset of client `Spell.dbc` (4.3.4) fields loaded for server-side spell logic.
/// Field indices follow TCPP `SpellEntryfmt` / `SpellEntry` in `DBCStructure.h`.
struct SpellDefinition {
  uint32 id = 0;
  uint32 attributes = 0;
  uint32 castingTimeIndex = 0;
  uint32 durationIndex = 0;
  uint32 powerType = 0;
  uint32 rangeIndex = 0;
  uint32 schoolMask = 0;
};

} // namespace Firelands
