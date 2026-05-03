#include "MySqlNpcTemplateSearchRepository.h"
#include <shared/Logger.h>

namespace Firelands {

MySqlNpcTemplateSearchRepository::MySqlNpcTemplateSearchRepository(
    std::shared_ptr<sql::Connection> connection)
    : m_connection(std::move(connection)) {}

std::vector<NpcTemplateSearchRow> MySqlNpcTemplateSearchRepository::SearchNameSubstring(
    std::string const &sanitizedQuery, uint32_t limit, uint32_t offset) const {
  std::vector<NpcTemplateSearchRow> out;
  if (!m_connection || sanitizedQuery.empty() || limit == 0)
    return out;

  try {
    std::unique_ptr<sql::PreparedStatement> pstmt(m_connection->prepareStatement(
        "SELECT `entry`, `name`, `subname` FROM `creature_template` "
        "WHERE LOWER(`name`) LIKE LOWER(CONCAT('%', ?, '%')) "
        "   OR LOWER(`subname`) LIKE LOWER(CONCAT('%', ?, '%')) "
        "ORDER BY `entry` ASC LIMIT ? OFFSET ?"));
    pstmt->setString(1, sanitizedQuery);
    pstmt->setString(2, sanitizedQuery);
    pstmt->setUInt(3, limit);
    pstmt->setUInt(4, offset);

    std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
    while (res->next()) {
      NpcTemplateSearchRow row;
      row.entry = res->getUInt("entry");
      row.name = res->getString("name");
      row.subname = res->getString("subname");
      out.push_back(std::move(row));
    }
  } catch (sql::SQLException &e) {
    LOG_WARN("NpcTemplateSearch query failed: {}", e.what());
  }
  return out;
}

} // namespace Firelands
