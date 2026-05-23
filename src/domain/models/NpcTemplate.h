#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Firelands {

/// Read-only `creature_template` snapshot for queries and GM tooling.
struct NpcTemplate {
  uint32_t entry = 0;
  std::string name;
  std::string subname;
  uint32_t factionTemplate = 0;
  uint32_t gossipMenuId = 0;
  uint64_t npcFlags = 0;
  uint32_t unitFieldFlags = 0;
  uint32_t unitFieldFlags2 = 0;
  uint32_t extraFlags = 0;
  std::vector<uint32_t> combatSpells;
  std::array<uint32_t, 4> displayIds{};
};

} // namespace Firelands
