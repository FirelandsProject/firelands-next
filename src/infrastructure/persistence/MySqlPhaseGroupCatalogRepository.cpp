#include "MySqlPhaseGroupCatalogRepository.h"

#include <shared/Logger.h>

#include <conncpp.hpp>

namespace Firelands {

MySqlPhaseGroupCatalogRepository::MySqlPhaseGroupCatalogRepository(
    std::shared_ptr<sql::Connection> connection)
    : m_connection(std::move(connection)) {}

std::unordered_map<uint32, std::vector<uint16>>
MySqlPhaseGroupCatalogRepository::LoadPhaseGroups() const {
  std::unordered_map<uint32, std::vector<uint16>> groups;
  if (!m_connection)
    return groups;

  size_t rowCount = 0;
  try {
    std::unique_ptr<sql::Statement> stmt(m_connection->createStatement());
    std::unique_ptr<sql::ResultSet> res(
        stmt->executeQuery("SELECT `PhaseGroupID`, `PhaseID` FROM `phase_x_phase_group`"));
    while (res->next()) {
      ++rowCount;
      uint32 const groupId = res->getUInt(1);
      uint16 const phaseId = static_cast<uint16>(res->getUInt(2));
      if (phaseId != 0)
        groups[groupId].push_back(phaseId);
    }
  } catch (sql::SQLException const &e) {
    LOG_WARN(
        "phase_x_phase_group not loaded ({}); PhaseGroup spawns need PhaseId or "
        "imported group data",
        e.what());
    return {};
  }

  if (rowCount == 0)
    LOG_WARN(
        "phase_x_phase_group is empty ({} rows); run migration 55 or "
        "`python3 tools/sql/import_ref_phase_data.py` and restart world",
        rowCount);
  else
    LOG_INFO(
        "Phase groups: {} row(s), {} distinct group id(s) from phase_x_phase_group",
        rowCount, groups.size());
  return groups;
}

} // namespace Firelands
