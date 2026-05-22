#pragma once

#include <domain/models/PlayerCreateInfo.h>
#include <domain/repositories/IPlayerCreateInfoRepository.h>
#include <cstdint>
#include <memory>
#include <shared/dbc/CharStartOutfitDbc.h>
#include <shared/dbc/GtOctCombatDbc.h>
#include <shared/dbc/GtPlayerStatGameTables.h>
#include <shared/dbc/StarterSpellsDbc.h>
#include <string>
#include <vector>

namespace Firelands {

class Character;

class PlayerCreateInfoService {
public:
  explicit PlayerCreateInfoService(
      std::shared_ptr<IPlayerCreateInfoRepository> repository,
      std::string charStartOutfitDbcPath = "",
      std::string clientGameTablesDbcDir = "");

  std::optional<PlayerCreateInfo> GetStartPosition(uint8 race, uint8 klass) {
    if (!m_repository)
      return std::nullopt;
    return m_repository->GetStartPosition(race, klass);
  }

  std::vector<PlayerCreateVisualItem> GetVisualItems(uint8 race, uint8 klass,
                                                     uint8 gender,
                                                     uint8 outfitId) {
    if (m_charStartOutfitDbcLoaded) {
      // Reference `DBCManager::GetCharStartOutfitEntry` ignores outfitId.
      auto dbcRows = m_charStartOutfitDbc.GetVisualItems(race, klass, gender);
      if (!dbcRows.empty())
        return dbcRows;
    }

    if (m_repository) {
      auto rows =
          m_repository->GetVisualItems(race, klass, gender, outfitId);
      if (!rows.empty())
        return rows;
    }

    return {};
  }

  /// Class spells from `playercreateinfo_spell` plus racial abilities from DBC when loaded.
  /// Riding/transport spells are omitted. Caller applies level filtering.
  std::vector<uint32_t> GetStarterSpells(uint8_t race, uint8_t klass) const;

  /// Race-restricted starter spells from DBC (empty when DBCs are not loaded).
  std::vector<uint32_t> GetRacialSpells(uint8_t race, uint8_t klass) const;

  /// Profession / meta skill-line spells from `SkillLineAbility.dbc` (not for login).
  bool IsSpellFromExcludedSkillLine(uint32_t spellId) const;

  std::vector<StarterSkillGrant> GetStarterSkills(uint8_t race, uint8_t klass) const;

  std::vector<StarterItemGrant>
  GetStarterItemGrants(uint8 race, uint8 klass, uint8 gender,
                       uint8 /*outfitId*/) {
    std::vector<StarterItemGrant> grants;
    if (m_charStartOutfitDbcLoaded) {
      auto dbcGrants =
          m_charStartOutfitDbc.GetStarterItemGrants(race, klass, gender);
      grants.insert(grants.end(), dbcGrants.begin(), dbcGrants.end());
    }
    if (m_repository) {
      auto extra = m_repository->GetExtraCreateItems(race, klass);
      grants.insert(grants.end(), extra.begin(), extra.end());
    }
    return grants;
  }

  /// Applies `player_classlevelstats` + `player_racestats` and optional `gtOCT*.dbc`
  /// (same sources as reference implementations). Returns false if no class row.
  bool TryApplyTemplateCombatState(Character &character);

  GtPlayerStatGameTables const &GetStatGameTables() const {
    return m_statGameTables;
  }

  /// XP to reach `level + 1` from world DB `player_xp_for_level`; fallback 400 if unset.
  uint32_t GetXpToNextLevelForLevel(uint8_t level) const;

private:
  std::shared_ptr<IPlayerCreateInfoRepository> m_repository;
  CharStartOutfitDbc m_charStartOutfitDbc;
  StarterSpellsDbc m_starterSpellsDbc;
  GtOctCombatDbc m_gtOct;
  GtPlayerStatGameTables m_statGameTables;
  bool m_charStartOutfitDbcLoaded = false;
  bool m_starterSpellsDbcLoaded = false;
};

} // namespace Firelands
