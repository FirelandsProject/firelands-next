#pragma once

#include <shared/Common.h>
#include <algorithm>
#include <random>

namespace Firelands {

/// Placeholder `CreatureDisplayInfo` id when spawn + template models are missing (client
/// still expects a non-zero display for many creature creates).
inline constexpr uint32 kFallbackCreatureDisplayId = 169u;

/// Spawn `creature.modelid` overrides template when non-zero; otherwise first non-zero
/// `modelid1`..`modelid4` from `creature_template` (Trinity layout).
inline uint32 ResolveCreatureDisplayId(uint32 spawnModelId, uint32 templateModelId1,
                                       uint32 templateModelId2, uint32 templateModelId3,
                                       uint32 templateModelId4) noexcept {
  if (spawnModelId != 0)
    return spawnModelId;
  if (templateModelId1 != 0)
    return templateModelId1;
  if (templateModelId2 != 0)
    return templateModelId2;
  if (templateModelId3 != 0)
    return templateModelId3;
  if (templateModelId4 != 0)
    return templateModelId4;
  return kFallbackCreatureDisplayId;
}

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
