#include "MySqlPhaseAreaCatalogRepository.h"

#include <shared/Logger.h>

#include <conncpp.hpp>

namespace Firelands {

MySqlPhaseAreaCatalogRepository::MySqlPhaseAreaCatalogRepository(
    std::shared_ptr<sql::Connection> connection)
    : m_connection(std::move(connection)) {}

std::unordered_map<uint32, std::vector<uint16>>
MySqlPhaseAreaCatalogRepository::LoadAreaPhases() const {
  std::unordered_map<uint32, std::vector<uint16>> areas;
  if (!m_connection)
    return areas;

  size_t rowCount = 0;
  try {
    std::unique_ptr<sql::Statement> stmt(m_connection->createStatement());
    std::unique_ptr<sql::ResultSet> res(
        stmt->executeQuery("SELECT `AreaId`, `PhaseId` FROM `phase_area`"));
    while (res->next()) {
      ++rowCount;
      uint32 const areaId = res->getUInt(1);
      uint16 const phaseId = static_cast<uint16>(res->getUInt(2));
      if (areaId != 0 && phaseId != 0)
        areas[areaId].push_back(phaseId);
    }
  } catch (sql::SQLException const &e) {
    LOG_WARN("phase_area not loaded ({}); zone-based player phasing disabled",
             e.what());
    return {};
  }

  if (rowCount == 0)
    LOG_WARN(
        "phase_area is empty ({} rows); run migration 55 or "
        "`python3 tools/sql/import_ref_phase_data.py` and restart world",
        rowCount);
  else
    LOG_INFO("Phase areas: {} row(s), {} distinct area id(s) from phase_area",
             rowCount, areas.size());
  return areas;
}

} // namespace Firelands
