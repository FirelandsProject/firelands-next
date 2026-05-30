#pragma once

#include <cmath>

namespace Firelands {

/// Cataclysm-style melee reach: `BASE_MELEE_RANGE` + both units' combat reach (+ small slop).
constexpr float kBaseMeleeRangeYards = 5.0f;
constexpr float kDefaultUnitCombatReachYards = 1.5f;
/// Small latency / rounding tolerance (not a full extra yard band).
constexpr float kMeleeRangeSlopYards = 0.5f;
constexpr float kMeleeMaxVerticalSlopYards = 3.0f;
/// Small tolerance for NPC spline vs server tick timing (not a full extra yard band).
constexpr float kNpcMeleePositionSyncToleranceYards = 0.5f;
constexpr float kNpcMeleeMaxVerticalSlopYards = 5.0f;

inline float MeleeRangeMaxYards(float attackerCombatReachYards,
                                float victimCombatReachYards) {
  return kBaseMeleeRangeYards + attackerCombatReachYards + victimCombatReachYards +
         kMeleeRangeSlopYards;
}

inline float MeleeRangeMaxSquared2d(
    float attackerCombatReachYards = kDefaultUnitCombatReachYards,
    float victimCombatReachYards = kDefaultUnitCombatReachYards) {
  float const r = MeleeRangeMaxYards(attackerCombatReachYards, victimCombatReachYards);
  return r * r;
}

inline bool IsWithinMeleeRange2d(float ax, float ay, float bx, float by,
                                float attackerCombatReachYards = kDefaultUnitCombatReachYards,
                                float victimCombatReachYards = kDefaultUnitCombatReachYards) {
  float const dx = ax - bx;
  float const dy = ay - by;
  return (dx * dx + dy * dy) <=
         MeleeRangeMaxSquared2d(attackerCombatReachYards, victimCombatReachYards);
}

inline bool IsWithinMeleeRange3d(float ax, float ay, float az, float bx, float by,
                                 float bz,
                                 float attackerCombatReachYards = kDefaultUnitCombatReachYards,
                                 float victimCombatReachYards = kDefaultUnitCombatReachYards) {
  if (!IsWithinMeleeRange2d(ax, ay, bx, by, attackerCombatReachYards,
                            victimCombatReachYards))
    return false;
  return std::fabs(az - bz) <= kMeleeMaxVerticalSlopYards;
}

/// Melee vs a creature: strict contact range, tuned to avoid long-distance hits.
inline bool IsWithinMeleeRangeAgainstNpc(float playerX, float playerY, float playerZ,
                     float creatureX, float creatureY,
                     float creatureZ,
                     float attackerCombatReachYards =
                       kDefaultUnitCombatReachYards,
                     float victimCombatReachYards =
                       kDefaultUnitCombatReachYards) {
  (void)attackerCombatReachYards;
  (void)victimCombatReachYards;
  float const maxYards = kBaseMeleeRangeYards + kMeleeRangeSlopYards;
  float const maxSq = maxYards * maxYards;
  float const dx = playerX - creatureX;
  float const dy = playerY - creatureY;
  if ((dx * dx + dy * dy) > maxSq)
    return false;
  return std::fabs(playerZ - creatureZ) <= kMeleeMaxVerticalSlopYards;
}

} // namespace Firelands
