#pragma once

#include <shared/Common.h>

namespace Firelands {

/// `Spell.dbc` field `Attributes` (wowdev SpellAttr0).
namespace SpellAttr0 {
/// `SPELL_ATTR0_PASSIVE` — no cast button; aura applied from spell passives.
constexpr uint32 kPassive = 0x00000010u;
/// `SPELL_ATTR0_CANT_CANCEL` — player cannot remove via `CMSG_CANCEL_AURA`.
constexpr uint32 kCantCancel = 0x00000020u;
/// `SPELL_ATTR0_NEGATIVE_SPELL` — harmful / debuff-style classification when effects do not imply polarity.
constexpr uint32 kNegativeSpell = 0x00000100u;
/// `SPELL_ATTR0_AURA_IS_DEBUFF` (wowdev / vname) — most harmful auras carry this bit.
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

/// `Spell.dbc` field `AttributesEx8` (wowdev SpellAttributesEx8).
namespace SpellAttr8 {
/// Client requires `AFLAG_ANY_EFFECT_AMOUNT_SENT` and per-effect `int32` amounts.
constexpr uint32 kAuraSendAmount = 0x00001000u;
} // namespace SpellAttr8

} // namespace Firelands
