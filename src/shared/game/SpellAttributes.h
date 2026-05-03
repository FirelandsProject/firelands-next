#pragma once

#include <shared/Common.h>

namespace Firelands {

/// `Spell.dbc` field `Attributes` (wowdev SpellAttr0 / TCPP `SPELL_ATTR0`).
namespace SpellAttr0 {
/// `SPELL_ATTR0_NEGATIVE_SPELL` — harmful / debuff-style classification when effects do not imply polarity.
constexpr uint32 kNegativeSpell = 0x00000100u;
} // namespace SpellAttr0

namespace SpellAttr2 {

/// `Spell.dbc` field `AttributesEx2` (wowdev SpellAttr2 / TCPP `AttributesEx2`).
constexpr uint32 kIgnoreLineOfSight = 0x00000004u;

} // namespace SpellAttr2
} // namespace Firelands
