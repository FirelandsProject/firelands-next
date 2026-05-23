#include <application/world/PhaseGroupCatalog.h>

namespace Firelands {

void PhaseGroupCatalog::Load(std::unordered_map<uint32, std::vector<uint16>> groups) {
  m_groups = std::move(groups);
}

std::vector<uint16> PhaseGroupCatalog::Resolve(uint32 phaseGroupId) const {
  if (phaseGroupId == 0)
    return {};
  auto const it = m_groups.find(phaseGroupId);
  if (it == m_groups.end())
    return {};
  return it->second;
}

} // namespace Firelands
