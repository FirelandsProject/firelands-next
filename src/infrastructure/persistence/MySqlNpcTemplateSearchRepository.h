#pragma once

#include <domain/repositories/INpcTemplateSearchRepository.h>
#include <conncpp.hpp>
#include <memory>

namespace Firelands {

class MySqlNpcTemplateSearchRepository final : public INpcTemplateSearchRepository {
public:
  explicit MySqlNpcTemplateSearchRepository(
      std::shared_ptr<sql::Connection> connection);

  std::vector<NpcTemplate> SearchNameSubstring(std::string const &sanitizedQuery,
                                               uint32_t limit,
                                               uint32_t offset) const override;

  std::optional<NpcTemplate> TryGetByEntry(uint32_t entry) const override;

private:
  std::shared_ptr<sql::Connection> m_connection;
};

} // namespace Firelands
