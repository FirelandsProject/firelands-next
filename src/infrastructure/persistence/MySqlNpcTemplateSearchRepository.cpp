#include "MySqlNpcTemplateSearchRepository.h"
#include <application/logic/CreatureSpawnLogic.h>
#include <shared/Logger.h>

namespace Firelands {

MySqlNpcTemplateSearchRepository::MySqlNpcTemplateSearchRepository(
    std::shared_ptr<sql::Connection> connection)
    : m_connection(std::move(connection)) {}

std::vector<NpcTemplate> MySqlNpcTemplateSearchRepository::SearchNameSubstring(
    std::string const &sanitizedQuery, uint32_t limit, uint32_t offset) const {
  std::vector<NpcTemplate> out;
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
      NpcTemplate row;
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

std::optional<NpcTemplate> MySqlNpcTemplateSearchRepository::TryGetByEntry(
    uint32_t entry) const {
  if (!m_connection || entry == 0)
    return std::nullopt;
  try {
    std::unique_ptr<sql::PreparedStatement> pstmt(m_connection->prepareStatement(
        "SELECT ct.`entry`, ct.`name`, ct.`subname`, ct.`faction`, "
        "ct.`gossip_menu_id`, ct.`npcflag`, ct.`unit_flags`, ct.`unit_flags2`, "
        "ct.`flags_extra`, "
        "(SELECT MIN(NULLIF(c.`modelid`, 0)) FROM `creature` c WHERE c.`id` = "
        "ct.`entry`) AS `spawn_modelid`, "
        "ct.`modelid1`, ct.`modelid2`, ct.`modelid3`, ct.`modelid4`, "
        "ct.`spell1`, ct.`spell2`, ct.`spell3`, ct.`spell4`, "
        "ct.`spell5`, ct.`spell6`, ct.`spell7`, ct.`spell8` "
        "FROM `creature_template` ct WHERE ct.`entry` = ? LIMIT 1"));
    pstmt->setUInt(1, entry);
    std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
    if (!res->next())
      return std::nullopt;
    NpcTemplate row;
    row.entry = res->getUInt("entry");
    row.name = res->getString("name");
    row.subname = res->getString("subname");
    if (!res->isNull("faction"))
      row.factionTemplate = res->getUInt("faction");
    if (!res->isNull("gossip_menu_id"))
      row.gossipMenuId = res->getUInt("gossip_menu_id");
    if (!res->isNull("npcflag"))
      row.npcFlags = res->getUInt64("npcflag");
    if (!res->isNull("unit_flags"))
      row.unitFieldFlags = res->getUInt("unit_flags");
    if (!res->isNull("unit_flags2"))
      row.unitFieldFlags2 = res->getUInt("unit_flags2");
    if (!res->isNull("flags_extra"))
      row.extraFlags = res->getUInt("flags_extra");
    uint32_t spawnModel = 0;
    if (!res->isNull("spawn_modelid"))
      spawnModel = res->getUInt("spawn_modelid");
    row.displayIds[0] = ResolveCreatureDisplayId(
        spawnModel, res->getUInt("modelid1"), res->getUInt("modelid2"),
        res->getUInt("modelid3"), res->getUInt("modelid4"));
    auto appendSpell = [&](char const *col) {
      if (res->isNull(col))
        return;
      uint32_t const id = res->getUInt(col);
      if (id != 0)
        row.combatSpells.push_back(id);
    };
    appendSpell("spell1");
    appendSpell("spell2");
    appendSpell("spell3");
    appendSpell("spell4");
    appendSpell("spell5");
    appendSpell("spell6");
    appendSpell("spell7");
    appendSpell("spell8");
    return row;
  } catch (sql::SQLException &e) {
    LOG_WARN("NpcTemplateSearch TryGetByEntry failed: {}", e.what());
  }
  return std::nullopt;
}

} // namespace Firelands
