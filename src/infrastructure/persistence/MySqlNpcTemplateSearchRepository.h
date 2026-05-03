#pragma once

#include <domain/repositories/INpcTemplateSearchRepository.h>
#include <conncpp.hpp>
#include <memory>

namespace Firelands {

class MySqlNpcTemplateSearchRepository final : public INpcTemplateSearchRepository {
public:
  explicit MySqlNpcTemplateSearchRepository(
      std::shared_ptr<sql::Connection> connection);

  std::vector<NpcTemplateSearchRow> SearchNameSubstring(
      std::string const &sanitizedQuery, uint32_t limit,
      uint32_t offset) const override;

private:
  std::shared_ptr<sql::Connection> m_connection;
};

} // namespace Firelands
