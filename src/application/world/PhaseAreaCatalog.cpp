#include <application/world/PhaseAreaCatalog.h>

namespace Firelands {

void PhaseAreaCatalog::Load(std::unordered_map<uint32, std::vector<uint16>> areaPhases) {
  m_areaPhases = std::move(areaPhases);
}

std::vector<uint16> PhaseAreaCatalog::ResolveForArea(uint32 areaId) const {
  if (areaId == 0)
    return {};
  auto const it = m_areaPhases.find(areaId);
  if (it == m_areaPhases.end())
    return {};
  return it->second;
}

} // namespace Firelands
