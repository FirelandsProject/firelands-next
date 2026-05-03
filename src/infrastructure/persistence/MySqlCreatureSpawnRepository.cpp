#include "MySqlCreatureSpawnRepository.h"
#include <shared/Logger.h>
#include <conncpp.hpp>

namespace Firelands {

MySqlCreatureSpawnRepository::MySqlCreatureSpawnRepository(
    std::shared_ptr<sql::Connection> connection)
    : m_connection(std::move(connection)) {}

std::vector<CreatureSpawnRow> MySqlCreatureSpawnRepository::LoadAllSpawns() const {
  std::vector<CreatureSpawnRow> out;
  if (!m_connection)
    return out;

  try {
    std::unique_ptr<sql::Statement> stmt(m_connection->createStatement());
    std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(
        "SELECT c.`guid`, c.`id`, c.`map`, c.`position_x`, c.`position_y`, "
        "c.`position_z`, c.`orientation`, c.`modelid`, "
        "ct.`unit_class`, ct.`minlevel`, ct.`maxlevel` "
        "FROM `creature` c "
        "INNER JOIN `creature_template` ct ON ct.`entry` = c.`id`"));
    while (res->next()) {
      CreatureSpawnRow row;
      row.guid = res->getUInt64("guid");
      row.entry = res->getUInt("id");
      row.mapId = res->getUInt("map");
      row.x = res->getDouble("position_x");
      row.y = res->getDouble("position_y");
      row.z = res->getDouble("position_z");
      row.orientation = static_cast<float>(res->getDouble("orientation"));
      row.modelId = res->getUInt("modelid");
      row.unitClass = static_cast<uint8>(res->getUInt("unit_class"));
      row.minLevel = static_cast<uint8>(res->getUInt("minlevel"));
      row.maxLevel = static_cast<uint8>(res->getUInt("maxlevel"));
      if (row.minLevel == 0)
        row.minLevel = 1;
      if (row.maxLevel == 0)
        row.maxLevel = row.minLevel;
      out.push_back(row);
    }
  } catch (sql::SQLException &e) {
    LOG_WARN("creature spawn query failed: {}", e.what());
  }
  return out;
}

} // namespace Firelands
