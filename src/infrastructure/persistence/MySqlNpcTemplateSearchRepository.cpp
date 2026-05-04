#include "MySqlNpcTemplateSearchRepository.h"
#include <application/logic/CreatureSpawnLogic.h>
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

std::optional<NpcTemplateSearchRow> MySqlNpcTemplateSearchRepository::TryGetByEntry(
    uint32_t entry) const {
  if (!m_connection || entry == 0)
    return std::nullopt;
  try {
    std::unique_ptr<sql::PreparedStatement> pstmt(m_connection->prepareStatement(
        "SELECT ct.`entry`, ct.`name`, ct.`subname`, ct.`faction`, "
        "(SELECT MIN(NULLIF(c.`modelid`, 0)) FROM `creature` c WHERE c.`id` = "
        "ct.`entry`) AS `spawn_modelid`, "
        "ct.`modelid1`, ct.`modelid2`, ct.`modelid3`, ct.`modelid4` "
        "FROM `creature_template` ct WHERE ct.`entry` = ? LIMIT 1"));
    pstmt->setUInt(1, entry);
    std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
    if (!res->next())
      return std::nullopt;
    NpcTemplateSearchRow row;
    row.entry = res->getUInt("entry");
    row.name = res->getString("name");
    row.subname = res->getString("subname");
    if (!res->isNull("faction"))
      row.factionTemplate = res->getUInt("faction");
    uint32_t spawnModel = 0;
    if (!res->isNull("spawn_modelid"))
      spawnModel = res->getUInt("spawn_modelid");
    row.displayIds[0] = ResolveCreatureDisplayId(
        spawnModel, res->getUInt("modelid1"), res->getUInt("modelid2"),
        res->getUInt("modelid3"), res->getUInt("modelid4"));
    return row;
  } catch (sql::SQLException &e) {
    LOG_WARN("NpcTemplateSearch TryGetByEntry failed: {}", e.what());
  }
  return std::nullopt;
}

} // namespace Firelands
