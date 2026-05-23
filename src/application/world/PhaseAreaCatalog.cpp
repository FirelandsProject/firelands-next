#include <application/world/PhaseAreaCatalog.h>

#include <algorithm>
#include <unordered_set>

namespace Firelands {

namespace {

void AppendUniquePhases(std::vector<uint16> &out, std::vector<uint16> const &add) {
  for (uint16 const phaseId : add) {
    if (phaseId == 0)
      continue;
    if (std::any_of(out.begin(), out.end(),
                    [phaseId](uint16 id) { return id == phaseId; })) {
      continue;
    }
    out.push_back(phaseId);
  }
}

} // namespace

void PhaseAreaCatalog::Load(std::unordered_map<uint32, std::vector<uint16>> areaPhases) {
  m_areaPhases = std::move(areaPhases);
}

std::vector<uint16> PhaseAreaCatalog::ResolveForArea(
    uint32 areaId, std::function<uint32(uint32)> parentOf) const {
  if (areaId == 0)
    return {};

  std::vector<uint16> phases;
  std::unordered_set<uint32> visited;
  uint32 current = areaId;
  constexpr size_t kMaxDepth = 32;

  for (size_t depth = 0; depth < kMaxDepth && current != 0; ++depth) {
    if (!visited.insert(current).second)
      break;

    if (auto const it = m_areaPhases.find(current); it != m_areaPhases.end())
      AppendUniquePhases(phases, it->second);

    if (!parentOf)
      break;

    current = parentOf(current);
  }

  return phases;
}

} // namespace Firelands
