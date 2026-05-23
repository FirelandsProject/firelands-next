#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Firelands {

/// Loads `phase_x_phase_group` rows keyed by `PhaseGroupID`.
class IPhaseGroupCatalogRepository {
public:
  virtual ~IPhaseGroupCatalogRepository() = default;

  virtual std::unordered_map<uint32, std::vector<uint16>> LoadPhaseGroups() const = 0;
};

} // namespace Firelands
