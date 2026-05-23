#pragma once

#include <application/world/PhaseAreaCatalog.h>
#include <application/world/PlayerQuestProgressStore.h>
#include <domain/models/PhaseCondition.h>
#include <domain/repositories/IPhaseAreaCatalogRepository.h>
#include <domain/repositories/IPhaseConditionRepository.h>

#include <unordered_map>
#include <vector>

namespace Firelands {

/// Merges `phase_area` rows with type-26 `conditions` into a catalog load map.
inline std::unordered_map<uint32, std::vector<PhaseAreaEntry>> BuildPhaseAreaCatalogLoadMap(
    std::unordered_map<uint32, std::vector<uint16>> const &areaPhases,
    PhaseConditionMap<PhaseConditionList> const &phaseConditions) {
  std::unordered_map<uint32, std::vector<PhaseAreaEntry>> out;
  for (auto const &[areaId, phaseIds] : areaPhases) {
    auto &entries = out[areaId];
    entries.reserve(phaseIds.size());
    for (uint16 const phaseId : phaseIds) {
      PhaseAreaEntry entry;
      entry.phaseId = phaseId;
      if (auto const it = phaseConditions.find(PhaseConditionKey{phaseId, areaId});
          it != phaseConditions.end())
        entry.conditions = it->second;
      entries.push_back(std::move(entry));
    }
  }
  return out;
}

} // namespace Firelands
