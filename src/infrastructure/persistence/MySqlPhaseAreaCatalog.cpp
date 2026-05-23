#include <application/world/PhaseAreaCatalog.h>

#include <shared/Logger.h>

#include <conncpp.hpp>

namespace Firelands {

void LoadPhaseAreaCatalogFromConnection(sql::Connection &connection,
                                         PhaseAreaCatalog &out) {
  std::unordered_map<uint32, std::vector<uint16>> areas;
  try {
    std::unique_ptr<sql::Statement> stmt(connection.createStatement());
    std::unique_ptr<sql::ResultSet> res(
        stmt->executeQuery("SELECT `AreaId`, `PhaseId` FROM `phase_area`"));
    while (res->next()) {
      uint32 const areaId = res->getUInt("AreaId");
      uint16 const phaseId = static_cast<uint16>(res->getUInt("PhaseId"));
      if (areaId != 0 && phaseId != 0)
        areas[areaId].push_back(phaseId);
    }
  } catch (sql::SQLException const &) {
    LOG_DEBUG("phase_area not loaded (table missing); zone-based player phasing disabled");
    out.Load({});
    return;
  }
  out.Load(std::move(areas));
  LOG_INFO("Phase areas: {} area id(s) from phase_area", areas.size());
}

} // namespace Firelands
