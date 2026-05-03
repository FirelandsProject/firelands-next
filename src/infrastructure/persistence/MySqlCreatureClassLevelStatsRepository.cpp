#include "MySqlCreatureClassLevelStatsRepository.h"
#include <application/logic/CreatureSpawnLogic.h>
#include <shared/Logger.h>
#include <algorithm>
#include <conncpp.hpp>

namespace Firelands {

static constexpr uint32 PackLevelClass(uint8 level, uint8 unitClass) noexcept {
  return (static_cast<uint32>(level) << 8u) | static_cast<uint32>(unitClass);
}

MySqlCreatureClassLevelStatsRepository::MySqlCreatureClassLevelStatsRepository(
    std::shared_ptr<sql::Connection> connection)
    : m_connection(std::move(connection)) {
  if (!m_connection)
    return;

  try {
    std::unique_ptr<sql::Statement> stmt(
        m_connection->createStatement());
    std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(
        "SELECT `level`, `class`, `basehealth` FROM `creature_classlevelstats`"));
    while (res->next()) {
      uint8 const level = static_cast<uint8>(res->getUInt("level"));
      uint8 const cls = static_cast<uint8>(res->getUInt("class"));
      uint32 const hp = res->getUInt("basehealth");
      m_healthByLevelClass[PackLevelClass(level, cls)] = hp;
    }
    LOG_INFO("creature_classlevelstats: loaded {} rows", m_healthByLevelClass.size());
  } catch (sql::SQLException &e) {
    LOG_WARN("creature_classlevelstats load failed (stats fall back to formula): {}",
             e.what());
    m_healthByLevelClass.clear();
  }
}

uint32 MySqlCreatureClassLevelStatsRepository::BaseHealthFor(uint8 level,
                                                               uint8 unitClass) const {
  uint8 const lv =
      static_cast<uint8>(std::max<uint32>(1u, std::min<uint32>(level, 255u)));
  uint8 const cls = NormalizeCreatureUnitClass(unitClass);
  auto it = m_healthByLevelClass.find(PackLevelClass(lv, cls));
  if (it != m_healthByLevelClass.end())
    return std::max<uint32>(1u, it->second);
  return FallbackCreatureBaseHealth(lv);
}

} // namespace Firelands
