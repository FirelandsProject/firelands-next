#pragma once

#include <cstdint>
#include <vector>

namespace Firelands {

/// Internal / UI meta skill lines from ref `playercreateinfo_skills` that must not
/// appear in PLAYER_SKILL update fields (client shows them as bogus professions).
bool IsMetaOrInternalStarterSkill(uint32_t skillId);

/// Riding and related skill lines granted only after training.
bool IsRidingStarterSkill(uint32_t skillId);

/// Primary and secondary profession skill lines (learned from trainers, not at create).
bool IsProfessionStarterSkill(uint32_t skillId);

/// Class spell-tab skill lines (e.g. "Mage - Fire") — not weapon/armor; clutter the skill UI.
bool IsClassSpellTabStarterSkill(uint32_t skillId);

/// Any starter skill that must not be sent on login.
bool IsExcludedStarterSkill(uint32_t skillId);

/// Guild perk spells (`SkillLine` 821 / category 5). Not granted until guilds exist.
bool IsGuildPerkSpell(uint32_t spellId);

/// Warlock demon summons taught by starter-zone quests (e.g. Piercing the Veil → 688),
/// not at character creation.
bool IsWarlockQuestGatedSummonSpell(uint32_t spellId);

/// Warlock quest-gated summon spell ids (for tests / quest-reward wiring).
std::vector<uint32_t> WarlockQuestGatedSummonSpellIds();

/// Riding, flying, and transport spells from `playercreateinfo` ref data that must not
/// be granted at character creation (learned later from trainers).
bool IsRidingOrTransportStarterSpell(uint32_t skillId);

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
