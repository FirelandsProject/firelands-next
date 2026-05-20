#pragma once

#include <cstdint>

namespace Firelands {

/// Riding, flying, and transport spells from `playercreateinfo` ref data that must not
/// be granted at character creation (learned later from trainers).
bool IsRidingOrTransportStarterSpell(uint32_t spellId);

/// Known player-mount spells (fallback when spell definitions are incomplete).
bool IsKnownMountSpell(uint32_t spellId);

/// Class shapeshift / travel form spells — belong in the spellbook but must not receive
/// a server aura on login (would force the wrong model).
bool IsClassShapeshiftStarterSpell(uint32_t spellId);

/// Mount / vehicle aura types from `SpellEffect.dbc` (`SPELL_AURA_MOUNTED`, etc.).
bool IsMountOrVehicleAuraType(uint32_t auraEffectType);

/// Aura types that must not be auto-applied on login (mount, shapeshift, vehicle).
bool IsExcludedLoginAuraType(uint32_t auraEffectType);

} // namespace Firelands
