#include "MySqlCreatureSpawnRepository.h"
#include <shared/Logger.h>
#include <conncpp.hpp>

namespace Firelands {

MySqlCreatureSpawnRepository::MySqlCreatureSpawnRepository(
    std::shared_ptr<sql::Connection> connection)
    : m_connection(std::move(connection)) {}

std::vector<CreatureSpawn> MySqlCreatureSpawnRepository::LoadAllSpawns() const {
  std::vector<CreatureSpawn> out;
  if (!m_connection)
    return out;

  try {
    std::unique_ptr<sql::Statement> stmt(m_connection->createStatement());
    std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(
        "SELECT c.`guid`, c.`id`, c.`map`, c.`position_x`, c.`position_y`, "
        "c.`position_z`, c.`orientation`, c.`modelid`, "
        "c.`phaseUseFlags`, c.`PhaseId`, c.`PhaseGroup`, "
        "ct.`modelid1`, ct.`modelid2`, ct.`modelid3`, ct.`modelid4`, "
        "ct.`unit_class`, ct.`minlevel`, ct.`maxlevel`, ct.`faction`, ct.`npcflag`, "
        "ct.`unit_flags`, ct.`unit_flags2`, ct.`flags_extra`, "
        "ct.`ExperienceModifier` "
        "FROM `creature` c "
        "INNER JOIN `creature_template` ct ON ct.`entry` = c.`id`"));
    while (res->next()) {
      CreatureSpawn row;
      row.guid = res->getUInt64("guid");
      row.entry = res->getUInt("id");
      row.mapId = res->getUInt("map");
      row.x = res->getDouble("position_x");
      row.y = res->getDouble("position_y");
      row.z = res->getDouble("position_z");
      row.orientation = static_cast<float>(res->getDouble("orientation"));
      row.modelId = res->getUInt("modelid");
      if (!res->isNull("phaseUseFlags"))
        row.phaseUseFlags = static_cast<uint8>(res->getUInt("phaseUseFlags"));
      if (!res->isNull("PhaseId"))
        row.phaseId = static_cast<uint16>(res->getUInt("PhaseId"));
      if (!res->isNull("PhaseGroup"))
        row.phaseGroup = res->getUInt("PhaseGroup");
      row.templateModelId1 = res->getUInt("modelid1");
      row.templateModelId2 = res->getUInt("modelid2");
      row.templateModelId3 = res->getUInt("modelid3");
      row.templateModelId4 = res->getUInt("modelid4");
      row.unitClass = static_cast<uint8>(res->getUInt("unit_class"));
      row.minLevel = static_cast<uint8>(res->getUInt("minlevel"));
      row.maxLevel = static_cast<uint8>(res->getUInt("maxlevel"));
      if (!res->isNull("faction"))
        row.factionTemplate = res->getUInt("faction");
      if (!res->isNull("npcflag"))
        row.npcFlags = static_cast<uint32>(res->getUInt64("npcflag"));
      if (!res->isNull("unit_flags"))
        row.unitFieldFlags = res->getUInt("unit_flags");
      if (!res->isNull("unit_flags2"))
        row.unitFieldFlags2 = res->getUInt("unit_flags2");
      if (!res->isNull("flags_extra"))
        row.extraFlags = res->getUInt("flags_extra");
      if (!res->isNull("ExperienceModifier"))
        row.experienceModifier = static_cast<float>(res->getDouble("ExperienceModifier"));
      if (row.experienceModifier <= 0.0f)
        row.experienceModifier = 1.0f;
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
