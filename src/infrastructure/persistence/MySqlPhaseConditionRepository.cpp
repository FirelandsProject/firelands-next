#include "MySqlPhaseConditionRepository.h"

#include <shared/Logger.h>

namespace Firelands {

namespace {

constexpr int kConditionSourceTypePhase = 26;

PhaseConditionType MapConditionType(int raw) {
  switch (raw) {
  case 1:
    return PhaseConditionType::Aura;
  case 8:
    return PhaseConditionType::QuestRewarded;
  case 9:
    return PhaseConditionType::QuestTaken;
  case 28:
    return PhaseConditionType::QuestComplete;
  default:
    return PhaseConditionType::None;
  }
}

} // namespace

MySqlPhaseConditionRepository::MySqlPhaseConditionRepository(
    std::shared_ptr<sql::Connection> connection)
    : m_connection(std::move(connection)) {}

PhaseConditionMap<PhaseConditionList>
MySqlPhaseConditionRepository::LoadPhaseConditions() const {
  PhaseConditionMap<PhaseConditionList> conditions;
  if (!m_connection)
    return conditions;

  size_t rowCount = 0;
  try {
    std::unique_ptr<sql::Statement> stmt(m_connection->createStatement());
    std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(
        "SELECT `SourceGroup`, `SourceEntry`, `ElseGroup`, "
        "`ConditionTypeOrReference`, `ConditionValue1`, `ConditionValue2`, "
        "`ConditionValue3`, `NegativeCondition` "
        "FROM `conditions` WHERE `SourceTypeOrReferenceId` = " +
        std::to_string(kConditionSourceTypePhase)));
    while (res->next()) {
      ++rowCount;
      uint16 const phaseId = static_cast<uint16>(res->getUInt(1));
      uint32 const areaId = res->getUInt(2);
      PhaseCondition row;
      row.elseGroup = res->getUInt(3);
      row.type = MapConditionType(res->getInt(4));
      row.value1 = res->getUInt(5);
      row.value2 = res->getUInt(6);
      row.value3 = res->getUInt(7);
      row.negative = res->getUInt(8) != 0;
      if (phaseId != 0 && areaId != 0 && row.type != PhaseConditionType::None)
        conditions[PhaseConditionKey{phaseId, areaId}].push_back(row);
    }
  } catch (sql::SQLException const &e) {
    LOG_WARN("phase conditions not loaded ({}); quest-gated phasing disabled", e.what());
    return {};
  }

  if (rowCount == 0)
    LOG_WARN(
        "conditions has no phase rows (type 26); run migration 57 or "
        "`python3 tools/sql/import_ref_phase_conditions.py` and restart world");
  else
    LOG_INFO("Phase conditions: {} row(s), {} phase/area pair(s)", rowCount,
             conditions.size());
  return conditions;
}

} // namespace Firelands
