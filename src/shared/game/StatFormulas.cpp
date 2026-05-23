#include <shared/game/StatFormulas.h>

#include <shared/dbc/GtPlayerStatGameTables.h>

#include <algorithm>

namespace Firelands {
namespace StatFormulas {

namespace {

/// `CombatRating` indices used with `gtCombatRatings.dbc`.
enum CombatRatingIndex : uint8_t {
  kCrDodge = 2,
  kCrParry = 3,
  kCrHitMelee = 5,
  kCrHitRanged = 6,
  kCrHitSpell = 7,
  kCrCritMelee = 8,
  kCrCritRanged = 9,
  kCrCritSpell = 10,
  kCrHasteMelee = 17,
  kCrHasteRanged = 18,
  kCrHasteSpell = 19,
  kCrMastery = 25,
};

uint8_t ClampPlayerLevel(uint8_t level) {
  if (level < 1u)
    return 1u;
  if (level > 85u)
    return 85u;
  return level;
}

size_t ClassTableIndex(uint8_t classId) {
  if (classId >= 1u && classId <= 9u)
    return static_cast<size_t>(classId) - 1u;
  if (classId == 11u)
    return 10u;
  return 0u;
}

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

float DodgeRatingPerPercentFallback(uint8_t level) {
  uint8_t const L = ClampPlayerLevel(level);
  if (L <= 60u) {
    float const t = (L <= 1u) ? 0.f : static_cast<float>(L - 1) / 59.f;
    return Lerp(10.5f, 27.52f, std::min(1.f, t));
}
  if (L <= 80u) {
    float const t = static_cast<float>(L - 60) / 20.f;
    return Lerp(27.52f, 43.944f, std::min(1.f, t));
}
  float const t = static_cast<float>(L - 80) / 5.f;
  return Lerp(43.944f, 179.28f, std::min(1.f, t));
}

float CombatRatingPerPointFallback(uint8_t crIndex, uint8_t level) {
  float const dodge = DodgeRatingPerPercentFallback(level);
  switch (crIndex) {
  case kCrHitMelee:
  case kCrHitRanged:
  case kCrHitSpell:
    return dodge * (120.109f / 179.28f);
  case kCrHasteMelee:
  case kCrHasteRanged:
  case kCrHasteSpell:
    return dodge * (128.057f / 179.28f);
  default:
    return dodge;
}
}

float RatingToPercent(uint8_t level, uint32_t rating, uint8_t crIndex,
                           GtPlayerStatGameTables const *gt) {
  if (rating == 0u)
    return 0.f;
  float per = 0.f;
  if (gt && gt->HasCombatRatings())
    per = gt->CombatRatingPerPercent(crIndex, level);
  if (per <= 0.f)
    per = CombatRatingPerPointFallback(crIndex, level);
  if (per <= 0.f)
    return 0.f;
  return static_cast<float>(rating) / per;
}

constexpr float kDodgeBaseFrac[11] = {
  0.037580f, 0.036520f, -0.054500f, -0.005900f, 0.031830f, 0.036640f,
  0.016750f, 0.034575f, 0.020350f, 0.0f, 0.049510f,
};

constexpr float kCritToDodge[11] = {
    0.85f / 1.15f, 1.00f / 1.15f, 1.11f / 1.15f, 2.00f / 1.15f, 1.00f / 1.15f,
    0.85f / 1.15f, 1.60f / 1.15f, 1.00f / 1.15f, 0.97f / 1.15f, 0.0f,
    2.00f / 1.15f,
};

constexpr float kSpellCritBaseFrac[11] = {
    0.0387f, 0.0341f, 0.0352f, 0.0349f, 0.0311f, 0.0392f,
    0.0339f, 0.0137f, 0.0219f, 0.0270f, 0.0295f,
};

float SpellCritRatioPerIntellectAtLevelFallback(uint8_t level) {
  uint8_t const L = ClampPlayerLevel(level);
  float const t = static_cast<float>(L - 1) / 84.f;
  return Lerp(0.00020f, 0.000038f, std::min(1.f, t));
}

float MeleeCritFromAgilityPerPointFallback(uint8_t level) {
  return SpellCritRatioPerIntellectAtLevelFallback(level) * 1.08f;
}

float MeleeCritRatioPerAgi(uint8_t level, uint8_t classId,
                           GtPlayerStatGameTables const *gt) {
  if (gt && gt->HasMeleeCritTable()) {
    float const r = gt->ChanceToMeleeCrit(classId, level);
    if (r > 0.f)
      return r;
}
  return MeleeCritFromAgilityPerPointFallback(level);
}

} // namespace

float AvoidanceAfterDiminishingReturns(float cap, float k, float nondiminishingPct,
                                       float diminishingPct) {
  if (cap <= 0.f || k <= 0.f)
    return std::max(0.f, nondiminishingPct);
  float const c = cap;
  float const x = std::max(0.f, diminishingPct);
  float const dr = (c * x) / (x + c * k);
  return dr + nondiminishingPct;
}

AvoidanceClassParams AvoidanceParamsForClass(uint8_t classId) {
  static constexpr float kDodgeCap[11] = {
  65.631440f, 65.631440f, 145.560408f, 145.560408f, 150.375940f, 65.631440f,
      145.560408f, 150.375940f, 150.375940f, 145.560408f, 116.890707f,
};
  static constexpr float kParryCap[11] = {
  65.631440f, 65.631440f, 145.560408f, 145.560408f, 0.0f, 65.631440f,
  145.560408f, 0.0f, 0.0f, 90.6425f, 0.0f,
};
  static constexpr float kDimK[11] = {
      0.9560f, 0.9560f, 0.9880f, 0.9880f, 0.9830f, 0.9560f,
      0.9880f, 0.9830f, 0.9830f, 0.9830f, 0.9720f,
};
  size_t const i = ClassTableIndex(classId);
  return AvoidanceClassParams{kDodgeCap[i], kParryCap[i], kDimK[i]};
}

bool ClassHasBaselineParry(uint8_t classId) {
  return classId == 1u || classId == 2u || classId == 4u || classId == 6u;
}

void ComputeDodgeContributionsFromAgility(uint8_t level, uint8_t classId,
                                          uint32_t agility, float &outDiminishingPct,
                                          float &outNondiminishingPct,
                           GtPlayerStatGameTables const *gt) {
  size_t const i = ClassTableIndex(classId);
  float const dodgeBaseFrac = kDodgeBaseFrac[i];
  float const nondimBase = std::max(0.f, 100.f * dodgeBaseFrac);
  float const ratio = MeleeCritRatioPerAgi(level, classId, gt);
  float const agiDodge =
      100.f * static_cast<float>(agility) * ratio * kCritToDodge[i];
  outNondiminishingPct = nondimBase;
  outDiminishingPct = std::max(0.f, agiDodge);
}

float DodgeRatingToPercent(uint8_t level, uint32_t dodgeRating,
                           GtPlayerStatGameTables const *gt) {
  return RatingToPercent(level, dodgeRating, kCrDodge, gt);
}

float ParryRatingToPercent(uint8_t level, uint32_t parryRating,
                           GtPlayerStatGameTables const *gt) {
  return RatingToPercent(level, parryRating, kCrParry, gt);
}

float PhysicalCritPercent(uint8_t level, uint8_t classId, uint32_t agility,
                          uint32_t critMeleeRating,
                           GtPlayerStatGameTables const *gt) {
  float const fromAgi =
      static_cast<float>(agility) * MeleeCritRatioPerAgi(level, classId, gt) * 100.f;
  float const fromRating =
      RatingToPercent(level, critMeleeRating, kCrCritMelee, gt);
  return 5.0f + fromAgi + fromRating;
}

float SpellCritPercent(uint8_t level, uint8_t classId, uint32_t intellect,
                       uint32_t critSpellRating,
                           GtPlayerStatGameTables const *gt) {
  size_t const i = ClassTableIndex(classId);
  float base = kSpellCritBaseFrac[i];
  float ratio = SpellCritRatioPerIntellectAtLevelFallback(level);
  if (gt && gt->HasSpellCritTables()) {
    base = gt->ChanceToSpellCritBase(classId);
    float const r = gt->ChanceToSpellCrit(classId, level);
    if (r > 0.f)
      ratio = r;
}
  float const fromInt = (base + static_cast<float>(intellect) * ratio) * 100.f;
  float const fromRating =
      RatingToPercent(level, critSpellRating, kCrCritSpell, gt);
  return fromInt + fromRating;
}

float MasteryPercentFromRating(uint8_t level, uint32_t masteryRating,
                           GtPlayerStatGameTables const *gt) {
  return RatingToPercent(level, masteryRating, kCrMastery, gt);
}

float MeleeHitPercentFromRating(uint8_t level, uint32_t hitRating,
                           GtPlayerStatGameTables const *gt) {
  return RatingToPercent(level, hitRating, kCrHitMelee, gt);
}

float SpellHitPercentFromRating(uint8_t level, uint32_t hitRating,
                           GtPlayerStatGameTables const *gt) {
  return RatingToPercent(level, hitRating, kCrHitSpell, gt);
}

float MeleeHastePercentFromRating(uint8_t level, uint32_t hasteRating,
                           GtPlayerStatGameTables const *gt) {
  return RatingToPercent(level, hasteRating, kCrHasteMelee, gt);
}

} // namespace StatFormulas
} // namespace Firelands
