#pragma once

#include <domain/repositories/ICreatureSpawnRepository.h>
#include <memory>

namespace sql {
class Connection;
}

namespace Firelands {

class MySqlCreatureSpawnRepository final : public ICreatureSpawnRepository {
public:
  explicit MySqlCreatureSpawnRepository(std::shared_ptr<sql::Connection> connection);

  std::vector<CreatureSpawnRow> LoadAllSpawns() const override;

private:
  std::shared_ptr<sql::Connection> m_connection;
};

} // namespace Firelands
