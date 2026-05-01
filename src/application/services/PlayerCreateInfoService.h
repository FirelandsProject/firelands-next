#pragma once

#include <domain/models/PlayerCreateInfo.h>
#include <domain/repositories/IPlayerCreateInfoRepository.h>
#include <memory>
#include <shared/dbc/CharStartOutfitDbc.h>
#include <string>

namespace Firelands {

class PlayerCreateInfoService {
public:
  explicit PlayerCreateInfoService(
      std::shared_ptr<IPlayerCreateInfoRepository> repository,
      std::string charStartOutfitDbcPath = "")
      : m_repository(std::move(repository)) {
    if (!charStartOutfitDbcPath.empty())
      m_charStartOutfitDbcLoaded =
          m_charStartOutfitDbc.Load(charStartOutfitDbcPath);
  }

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

  std::vector<StarterItemGrant>
  GetStarterItemGrants(uint8 race, uint8 klass, uint8 gender,
                       uint8 /*outfitId*/) {
    std::vector<StarterItemGrant> grants;
    if (m_charStartOutfitDbcLoaded) {
      for (uint32_t id :
           m_charStartOutfitDbc.GetStarterItemIds(race, klass, gender)) {
        StarterItemGrant g;
        g.itemId = id;
        g.count = 0; // resolved from item proto (`buy_count`) at grant time
        grants.push_back(g);
      }
    }
    if (m_repository) {
      auto extra = m_repository->GetExtraCreateItems(race, klass);
      grants.insert(grants.end(), extra.begin(), extra.end());
    }
    return grants;
  }

private:
  std::shared_ptr<IPlayerCreateInfoRepository> m_repository;
  CharStartOutfitDbc m_charStartOutfitDbc;
  bool m_charStartOutfitDbcLoaded = false;
};

} // namespace Firelands
