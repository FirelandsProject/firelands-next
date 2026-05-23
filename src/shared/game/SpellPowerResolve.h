#pragma once

#include <shared/Common.h>
#include <algorithm>
#include <cmath>

namespace Firelands {

/// Parsed row from `SpellPower.dbc` (`SpellPowerEntryfmt` = "diiiixxf", build 15595).
struct SpellPowerDbcRow {
 uint32 flatCost = 0;
 uint32 costPerLevel = 0;
 /// `CostPercentage` column — % of caster base POWER1 (`GetCreateMana` pool).
 uint32 costPercent = 0;
 /// Trailing float when the integer percentage column is zero (common on mage rows).
 float costPercentFloat = 0.f;
};

/// Resolves POWER1 cost at cast time from `SpellPower.dbc` and `Spell.dbc` `PowerType`.
uint32 ResolveSpellPowerCost(SpellPowerDbcRow const &row, uint32 spellPowerType,
 uint8 casterLevel, uint8 spellLevel,
 uint32 power1PoolForPercent);

} // namespace Firelands
