#pragma once

#include <shared/Common.h>
#include <algorithm>
#include <random>

namespace Firelands {

inline uint8 NormalizeCreatureUnitClass(uint8 unitClass) noexcept {
  return unitClass == 0 ? 1 : unitClass;
}

inline uint8 PickCreatureLevelInclusive(uint8 minLevel, uint8 maxLevel,
                                        std::mt19937 &rng) noexcept {
  uint16 lo = minLevel;
  uint16 hi = maxLevel;
  if (lo > hi)
    std::swap(lo, hi);
  hi = static_cast<uint16>(std::min<uint32>(hi, 255u));
  lo = static_cast<uint16>(std::min<uint32>(lo, hi));
  std::uniform_int_distribution<uint16> dist(lo, hi);
  return static_cast<uint8>(dist(rng));
}

inline uint32 FallbackCreatureBaseHealth(uint8 level) noexcept {
  uint32 const lv = std::max<uint32>(1u, std::min<uint32>(level, 255u));
  return 45u + lv * 15u;
}

} // namespace Firelands
