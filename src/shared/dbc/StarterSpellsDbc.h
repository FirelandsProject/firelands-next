#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace Firelands {

/// Resolves starter spells from SkillRaceClassInfo + SkillLineAbility (DBC).
/// `GetStarterSpells` returns class + racial spells; `GetRacialSpells` returns
/// only race-restricted SkillLineAbility rows for merging with world DB data.
class StarterSpellsDbc {
public:
  bool Load(std::string const &skillLineAbilityPath,
            std::string const &skillRaceClassInfoPath);

  bool IsLoaded() const { return m_loaded; }

  std::vector<uint32_t> GetStarterSpells(uint8_t race, uint8_t klass) const;

  /// Weapon, armor, and language skill-line abilities for login (not class tabs).
  std::vector<uint32_t> GetWeaponArmorLanguageStarterSpells(uint8_t race,
                                                            uint8_t klass) const;

  /// Race-specific abilities (non-zero `raceMask` in SkillLineAbility.dbc).
  std::vector<uint32_t> GetRacialSpells(uint8_t race, uint8_t klass) const;

  /// True when `spellId` is granted via a profession / meta skill line in DBC.
  bool IsSpellFromExcludedSkillLine(uint32_t spellId) const;

private:
  std::vector<uint32_t> collectSkillLineSpells(uint8_t race, uint8_t klass,
                                             bool weaponArmorLanguageOnly) const;

  std::vector<uint32_t>
  finalizeCandidates(std::unordered_set<uint32_t> candidates) const;

  struct SkillLineAbilityRow {
    uint32_t skillLine = 0;
    uint32_t spellId = 0;
    uint32_t raceMask = 0;
    uint32_t classMask = 0;
    uint32_t minSkillLineRank = 0;
    uint32_t supercededBySpell = 0;
    uint32_t acquireMethod = 0;
  };

  bool m_loaded = false;
  std::vector<SkillLineAbilityRow> m_abilities;
  std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> m_skillRaceClass;
};

} // namespace Firelands
