#pragma once

#include <cstdint>
#include <string>

namespace Firelands {

/// SkillLine.dbc `categoryId` values (Cataclysm 4.3.4, `SkillLineCategory`).
namespace SkillLineCategory {
/// `SkillLine.dbc` categoryId (Cataclysm 4.3.4); ref `SkillLineCategory`.
constexpr uint32_t Guild = 5u;
constexpr uint32_t Weapon = 6u;
constexpr uint32_t Class = 7u;
constexpr uint32_t Armor = 8u;
constexpr uint32_t Secondary = 9u;
constexpr uint32_t Language = 10u;
constexpr uint32_t Profession = 11u;
constexpr uint32_t Generic = 12u;
} // namespace SkillLineCategory

/// Loads `SkillLine.dbc` for category-based starter skill filtering.
bool LoadSkillLineCategories(std::string const &skillLineDbcPath);
bool SkillLineCategoriesLoaded();

/// First Aid / Cooking / Fishing / Riding — category 9 in DBC, must not fill PLAYER_SKILL slots.
bool IsSecondaryProfessionSkillLine(uint32_t skillId);

/// Starter characters only receive weapon, armor, and language skill lines on the wire
/// (PLAYER_SKILL_* update fields). Categories 6, 8, 10 only.
bool IsAllowedStarterSkillLine(uint32_t skillId);

/// Spells from these skill lines must NOT be in the starter spellbook.
/// Blocks guild perks (cat 5), professions (cat 11), and generic/DND (cat 12).
/// Weapon (6), armor (8), language (10), class (7), and racial/secondary (9) allowed.
bool IsExcludedSpellGrantSkillLine(uint32_t skillId);

/// Loads `SkillLineAbility.dbc` spell → skill line index for login filters.
bool LoadSkillLineAbilitySpellIndex(std::string const &skillLineAbilityDbcPath);

/// True when `spellId` is granted via guild/profession/generic skill lines in DBC.
bool IsSpellFromExcludedSkillLine(uint32_t spellId);

} // namespace Firelands
