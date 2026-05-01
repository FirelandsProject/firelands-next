#pragma once

#include <domain/repositories/IPlayerCreateInfoRepository.h>
#include <conncpp.hpp>
#include <memory>

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

private:
  std::shared_ptr<sql::Connection> m_connection;
};

} // namespace Firelands
