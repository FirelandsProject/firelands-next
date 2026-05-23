#pragma once

#include <domain/repositories/IPlayerCreateInfoRepository.h>
#include <conncpp.hpp>
#include <cstdint>
#include <memory>
#include <vector>

namespace Firelands {

class MySqlPlayerCreateInfoRepository : public IPlayerCreateInfoRepository {
public:
  explicit MySqlPlayerCreateInfoRepository(
      std::shared_ptr<sql::Connection> connection)
      : m_connection(std::move(connection)) {}

  std::optional<PlayerCreateInfo> GetStartPosition(uint8 race,
                                                   uint8 klass) override;

  std::vector<PlayerCreateVisualItem>
  GetVisualItems(uint8 race, uint8 klass, uint8 gender, uint8 outfitId) override;

  std::vector<StarterItemGrant> GetExtraCreateItems(uint8 race,
                                                     uint8 klass) override;

  std::vector<uint32_t> GetStarterSpells(uint8_t race, uint8_t klass) override;
  std::vector<uint32_t> GetRacialStarterSpells(uint8_t race,
                                               uint8_t klass) override;
  std::vector<StarterSkillGrant> GetStarterSkills(uint8_t race,
                                                uint8_t klass) override;

  std::optional<PlayerClassLevelStats>
  GetClassLevelStats(uint8_t klass, uint8_t level) override;

  std::optional<PlayerRaceStats> GetRaceStats(uint8_t race) override;

  uint32_t GetXpForNextLevel(uint8_t currentLevel) const override;

private:
  void ensureXpForLevelLoaded() const;

  std::shared_ptr<sql::Connection> m_connection;
  mutable bool m_xpForLevelLoadAttempted = false;
  mutable std::vector<uint32_t> m_xpExperienceByLevel;
};

} // namespace Firelands
