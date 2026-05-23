#include <application/world/PhaseGroupCatalog.h>

#include <shared/Logger.h>

#include <conncpp.hpp>

namespace Firelands {

void LoadPhaseGroupCatalogFromConnection(sql::Connection &connection,
                                         PhaseGroupCatalog &out) {
  std::unordered_map<uint32, std::vector<uint16>> groups;
  try {
    std::unique_ptr<sql::Statement> stmt(connection.createStatement());
    std::unique_ptr<sql::ResultSet> res(
        stmt->executeQuery("SELECT `PhaseGroupID`, `PhaseID` FROM `phase_x_phase_group`"));
    while (res->next()) {
      uint32 const groupId = res->getUInt("PhaseGroupID");
      uint16 const phaseId = static_cast<uint16>(res->getUInt("PhaseID"));
      if (phaseId != 0)
        groups[groupId].push_back(phaseId);
    }
  } catch (sql::SQLException const &) {
    LOG_DEBUG(
        "phase_x_phase_group not loaded (table missing or empty); PhaseGroup spawns "
        "need PhaseId or imported group data");
    out.Load({});
    return;
  }
  out.Load(std::move(groups));
  LOG_INFO("Phase groups: {} group(s) from phase_x_phase_group", groups.size());
}

} // namespace Firelands
