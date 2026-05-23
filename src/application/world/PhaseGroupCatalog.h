#pragma once

#include <shared/Common.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Firelands {

class PhaseGroupCatalog {
public:
  void Load(std::unordered_map<uint32, std::vector<uint16>> groups);
  std::vector<uint16> Resolve(uint32 phaseGroupId) const;

private:
  std::unordered_map<uint32, std::vector<uint16>> m_groups;
};

} // namespace Firelands
