#pragma once

#include <cstdint>

namespace Firelands {

/// Standing band sent in `SMSG_SET_FORCED_REACTIONS` (second `uint32` per pair).
/// Matches reference `ReputationRank` / client reputation tiers.
enum class ReputationRank : uint32_t {
  Hated = 0,
  Hostile = 1,
  Unfriendly = 2,
  Neutral = 3,
  Friendly = 4,
  Honored = 5,
  Revered = 6,
  Exalted = 7,
};

inline bool IsValidReputationRankValue(uint32_t v) {
  return v <= static_cast<uint32_t>(ReputationRank::Exalted);
}

} // namespace Firelands
