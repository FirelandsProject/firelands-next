#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Firelands {

/// Loads `phase_area` rows keyed by `AreaId`.
class IPhaseAreaCatalogRepository {
public:
  virtual ~IPhaseAreaCatalogRepository() = default;

  virtual std::unordered_map<uint32, std::vector<uint16>> LoadAreaPhases() const = 0;
};

} // namespace Firelands
