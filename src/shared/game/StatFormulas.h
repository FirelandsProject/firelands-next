#pragma once

#include <cstdint>

namespace Firelands {

class GtPlayerStatGameTables;

namespace StatFormulas {

/// Diminishing returns on avoidance (dodge / parry) from reference implementation
/// `StatSystem.cpp::CalculateDiminishingReturns`: \(x' = \frac{c x}{x + c k} + x_{nd}\).
float AvoidanceAfterDiminishingReturns(float cap, float k, float nondiminishingPct,
                                       float diminishingPct);

struct AvoidanceClassParams {
  float dodgeCap;
  float parryCap;
  float diminishingK;
};

/// Uses class index `classId` 1..11 (Blizzard `ChrClasses`; 10 unused).
AvoidanceClassParams AvoidanceParamsForClass(uint8_t classId);

bool ClassHasBaselineParry(uint8_t classId);

/// Pre-DR dodge from agility (dim) + class base dodge (non-dim), in **percent points**
/// (e.g. 5.0f = 5%). Uses `gtChanceToMeleeCrit` when `gt` is loaded.
void ComputeDodgeContributionsFromAgility(uint8_t level, uint8_t classId,
                                          uint32_t agility, float &outDiminishingPct,
                                          float &outNondiminishingPct,
                                          GtPlayerStatGameTables const *gt = nullptr);

float DodgeRatingToPercent(uint8_t level, uint32_t dodgeRating,
                           GtPlayerStatGameTables const *gt = nullptr);
float ParryRatingToPercent(uint8_t level, uint32_t parryRating,
                           GtPlayerStatGameTables const *gt = nullptr);

/// Melee / ranged crit before DR caps: 5% baseline (paper doll) + agility + crit rating.
float PhysicalCritPercent(uint8_t level, uint8_t classId, uint32_t agility,
                          uint32_t critMeleeRating,
                          GtPlayerStatGameTables const *gt = nullptr);

/// Spell crit for schools 1..6 (same value each): base + intellect + spell crit rating.
float SpellCritPercent(uint8_t level, uint8_t classId, uint32_t intellect,
                       uint32_t critSpellRating,
                       GtPlayerStatGameTables const *gt = nullptr);

float MasteryPercentFromRating(uint8_t level, uint32_t masteryRating,
                               GtPlayerStatGameTables const *gt = nullptr);

float MeleeHitPercentFromRating(uint8_t level, uint32_t hitRating,
                                GtPlayerStatGameTables const *gt = nullptr);
float SpellHitPercentFromRating(uint8_t level, uint32_t hitRating,
                                GtPlayerStatGameTables const *gt = nullptr);

float MeleeHastePercentFromRating(uint8_t level, uint32_t hasteRating,
                                  GtPlayerStatGameTables const *gt = nullptr);

} // namespace StatFormulas
} // namespace Firelands
