#pragma once

#include <shared/Common.h>

namespace Firelands {

/// `Spell.dbc` field `Attributes` (wowdev SpellAttr0).
namespace SpellAttr0 {
/// `SPELL_ATTR0_NEGATIVE_SPELL` — harmful / debuff-style classification when effects do not imply polarity.
constexpr uint32 kNegativeSpell = 0x00000100u;
/// `SPELL_ATTR0_AURA_IS_DEBUFF` (wowdev / vmangos name) — most harmful auras carry this bit.
constexpr uint32 kAuraIsDebuff = 0x04000000u;
} // namespace SpellAttr0

/// `Spell.dbc` field `AttributesEx` (wowdev SpellAttributesEx).
namespace SpellAttrEx {
constexpr uint32 kInitiatesCombat = 0x00000200u;
} // namespace SpellAttrEx

namespace SpellAttr2 {

/// `Spell.dbc` field `AttributesEx2` (wowdev SpellAttr2).
constexpr uint32 kIgnoreLineOfSight = 0x00000004u;

} // namespace SpellAttr2
} // namespace Firelands
