#pragma once

#include <domain/repositories/IPhaseAreaCatalogRepository.h>
#include <memory>

namespace sql {
class Connection;
}

namespace Firelands {

class MySqlPhaseAreaCatalogRepository final : public IPhaseAreaCatalogRepository {
public:
  explicit MySqlPhaseAreaCatalogRepository(std::shared_ptr<sql::Connection> connection);

  std::unordered_map<uint32, std::vector<uint16>> LoadAreaPhases() const override;

private:
  std::shared_ptr<sql::Connection> m_connection;
};

} // namespace Firelands
