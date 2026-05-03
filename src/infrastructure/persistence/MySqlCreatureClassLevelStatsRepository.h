#pragma once

#include <domain/repositories/ICreatureClassLevelStatsRepository.h>
#include <unordered_map>
#include <memory>

namespace sql {
class Connection;
}

namespace Firelands {

class MySqlCreatureClassLevelStatsRepository final
    : public ICreatureClassLevelStatsRepository {
public:
  explicit MySqlCreatureClassLevelStatsRepository(
      std::shared_ptr<sql::Connection> connection);

  uint32 BaseHealthFor(uint8 level, uint8 unitClass) const override;

private:
  std::shared_ptr<sql::Connection> m_connection;
  std::unordered_map<uint32, uint32> m_healthByLevelClass;
};

} // namespace Firelands
