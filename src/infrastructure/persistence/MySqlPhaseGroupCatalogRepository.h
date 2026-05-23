#pragma once

#include <domain/repositories/IPhaseGroupCatalogRepository.h>
#include <memory>

namespace sql {
class Connection;
}

namespace Firelands {

class MySqlPhaseGroupCatalogRepository final : public IPhaseGroupCatalogRepository {
public:
  explicit MySqlPhaseGroupCatalogRepository(std::shared_ptr<sql::Connection> connection);

  std::unordered_map<uint32, std::vector<uint16>> LoadPhaseGroups() const override;

private:
  std::shared_ptr<sql::Connection> m_connection;
};

} // namespace Firelands
