#pragma once

#include <shared/Common.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Firelands {

/// `phase_area` rows: phases applied to players by zone/area (TrinityCore `OnAreaChange`).
class PhaseAreaCatalog {
public:
  void Load(std::unordered_map<uint32, std::vector<uint16>> areaPhases);
  std::vector<uint16> ResolveForArea(uint32 areaId) const;

private:
  std::unordered_map<uint32, std::vector<uint16>> m_areaPhases;
};

} // namespace Firelands
