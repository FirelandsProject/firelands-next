#pragma once

#include <cstdint>
#include <vector>

namespace Firelands {

/// Guild perk spells (`SkillLine` 821 / category 5). Not granted until guilds exist.
bool IsGuildPerkSpell(uint32_t spellId);

/// Warlock demon summons taught by starter-zone quests (e.g. Piercing the Veil → 688),
/// not at character creation.
bool IsWarlockQuestGatedSummonSpell(uint32_t spellId);

/// Warlock quest-gated summon spell ids (for tests / quest-reward wiring).
std::vector<uint32_t> WarlockQuestGatedSummonSpellIds();

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

/// Beneficial always-on aura rows that may apply at login without `SPELL_ATTR0_PASSIVE`.
bool IsAlwaysOnLoginAuraType(uint32_t auraEffectType);

} // namespace Firelands
