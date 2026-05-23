#pragma once

#include <domain/models/PhaseCondition.h>
#include <domain/ports/IPlayerQuestProgress.h>
#include <shared/Common.h>

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace Firelands {

struct PhaseAreaEntry {
  uint16 phaseId = 0;
  PhaseConditionList conditions;
};

/// `phase_area` rows with optional `conditions` (TrinityCore `OnAreaChange`).
class PhaseAreaCatalog {
public:
  void Load(std::unordered_map<uint32, std::vector<PhaseAreaEntry>> areaPhases);
  /// Resolves active phases for `areaId`, walking parents via `parentOf` when provided.
  std::vector<uint16> ResolveForArea(
      uint32 areaId, IPlayerQuestProgress const &player,
      std::function<uint32(uint32)> parentOf = {}) const;

private:
  std::unordered_map<uint32, std::vector<PhaseAreaEntry>> m_areaPhases;
};

} // namespace Firelands
