#include <gtest/gtest.h>
#include <chrono>
#include <application/spell/SpellHitEffects.h>
#include <application/spell/SpellManager.h>
#include <domain/repositories/ISpellCastTables.h>
#include <shared/game/SpellAuraTypes.h>

using namespace Firelands;

namespace {

class DurationTablesStub final : public ISpellCastTables {
public:
  explicit DurationTablesStub(uint32 durationMs) : m_durationMs(durationMs) {}

  uint32 GetCastTimeMs(uint32) const override { return 0u; }
  float GetSpellRangeMinYards(uint32, bool) const override { return 0.f; }
  float GetSpellRangeMaxYards(uint32, bool) const override { return 0.f; }
  void GetCooldownTiming(uint32, uint32 *, uint32 *, uint32 *) const override {}
    uint32 ResolveSpellPowerCost(uint32, uint32, uint8, uint8, uint32) const override {
        return 0u;
}
  uint32 GetSpellCategoryGroupForCategoriesId(uint32) const override { return 0u; }
  uint32 GetDurationMs(uint32 durationIndex, uint8 /*casterLevel*/) const override {
    return durationIndex == m_index ? m_durationMs : 0u;
}

  uint32 m_index = 5;
  uint32 m_durationMs;
};

} // namespace

TEST(SpellHitEffectsTests, ResolvePrimarySpellHitSelfWhenNoUnitTargetBit) {
  uint64 const g = 0x100ULL;
  EXPECT_EQ(SpellHitEffects::ResolvePrimarySpellHitUnitGuid(0, g, 0x200ULL), g);
}

TEST(SpellHitEffectsTests, ResolvePrimarySpellHitUsesUnitWhenMaskedAndGuidNonZero) {
  uint64 const caster = 0x100ULL;
  uint64 const target = 0x200ULL;
  uint32 const flags = SpellCastWire::TARGET_FLAG_UNIT;
  EXPECT_EQ(SpellHitEffects::ResolvePrimarySpellHitUnitGuid(flags, caster, target),
            target);
}

TEST(SpellHitEffectsTests,
     ResolvePrimarySpellHitFallsBackToCasterWhenMaskedButUnitGuidZero) {
  uint64 const caster = 0x100ULL;
  uint32 const flags = SpellCastWire::TARGET_FLAG_UNIT;
  EXPECT_EQ(SpellHitEffects::ResolvePrimarySpellHitUnitGuid(flags, caster, 0u), caster);
}

TEST(SpellHitEffectsTests, ApplyImmediateHealthDoesNothingWhenDefNull) {
  SpellCastOutcome out{};
  out.hasDirectHealthEffect = false;
  SpellHitEffects::ApplyImmediateHealthFromDefinition(nullptr, 0x42ULL, &out);
  EXPECT_FALSE(out.hasDirectHealthEffect);
  EXPECT_EQ(out.directHealthTargetGuid, 0u);
  EXPECT_EQ(out.directHealthDelta, 0);
}

TEST(SpellHitEffectsTests, ApplyImmediateHealthDoesNothingWhenDeltaZero) {
  SpellDefinition def{};
  def.immediateHealthEffectDelta = 0;
  SpellCastOutcome out{};
  SpellHitEffects::ApplyImmediateHealthFromDefinition(&def, 0x99ULL, &out);
  EXPECT_FALSE(out.hasDirectHealthEffect);
}

TEST(SpellHitEffectsTests, ApplyImmediateHealthWritesOutcomeWhenDeltaNonZero) {
  SpellDefinition def{};
  def.immediateHealthEffectDelta = -42;
  SpellCastOutcome out{};
  uint64 const hit = 0xABCDULL;
  SpellHitEffects::ApplyImmediateHealthFromDefinition(&def, hit, &out);
  EXPECT_TRUE(out.hasDirectHealthEffect);
  EXPECT_EQ(out.directHealthTargetGuid, hit);
  EXPECT_EQ(out.directHealthDelta, -42);
}

TEST(SpellHitEffectsTests, ApplyAuraSetsOutcomeWhenDefinitionHasAura) {
  SpellDefinition def{};
  def.id = 139;
  def.hasAuraEffect = true;
  def.auraEffectType = 8;
  def.durationIndex = 5;
  def.auraDurationIndex = 5;
  DurationTablesStub tables(20000);
  SpellCastOutcome out{};
  auto const now = std::chrono::steady_clock::now();
  SpellHitEffects::ApplyAuraFromDefinition(&def, 0x50ULL, 0x40ULL, 80, now, &tables, &out);
  EXPECT_TRUE(out.hasAuraApply);
  EXPECT_EQ(out.auraTargetGuid, 0x50ULL);
  EXPECT_EQ(out.auraSpellId, 139u);
  EXPECT_EQ(out.auraDurationMs, 20000u);
}

TEST(SpellHitEffectsTests, ApplyAuraPropagatesPeriodicHealTickFromDefinition) {
  SpellDefinition def{};
  def.id = 774;
  def.hasAuraEffect = true;
  def.auraEffectType = 8;
  def.auraBasePoints = -4;
  def.auraPeriodicPeriodMs = 3000;
  SpellCastOutcome out{};
  SpellHitEffects::ApplyAuraFromDefinition(&def, 0x50ULL, 0x40ULL, 80,
                                           std::chrono::steady_clock::now(), nullptr,
                                           &out);
  ASSERT_TRUE(out.hasAuraApply);
  EXPECT_EQ(out.auraDurationMs, 0u);
  EXPECT_EQ(out.auraPeriodicPeriodMs, 3000u);
  EXPECT_EQ(out.auraPeriodicHealthDeltaPerTick, 4);
}

TEST(SpellHitEffectsTests, ResolveAuraDurationMs_ReplacesLegacyOneHourFallback) {
  SpellDefinition def{};
  def.durationIndex = 5;
  DurationTablesStub tables(15000);
  EXPECT_EQ(SpellHitEffects::ResolveAuraDurationMs(774, 80, 3600000u, &def, &tables),
            15000u);
}

TEST(SpellHitEffectsTests, ResolveAuraDurationMs_KeepsValidOutcomeDuration) {
  SpellDefinition def{};
  def.durationIndex = 5;
  DurationTablesStub tables(15000);
  EXPECT_EQ(SpellHitEffects::ResolveAuraDurationMs(774, 80, 12000u, &def, &tables),
            12000u);
}

TEST(SpellHitEffectsTests, ApplyAuraPeriodicHealUsesRealPointsPerLevelAtCasterLevel) {
  SpellDefinition def{};
  def.id = 139;
  def.hasAuraEffect = true;
  def.auraEffectType = 8;
  def.auraBasePoints = 0;
  def.auraRealPointsPerLevel = 1.25f;
  def.auraPeriodicPeriodMs = 3000;
  SpellCastOutcome out{};
  SpellHitEffects::ApplyAuraFromDefinition(&def, 0x50ULL, 0x40ULL, 80,
                                           std::chrono::steady_clock::now(), nullptr,
                                           &out);
  ASSERT_TRUE(out.hasAuraApply);
  EXPECT_EQ(out.auraPeriodicHealthDeltaPerTick, 100);
}

TEST(SpellHitEffectsTests, ApplyAuraNoOpWhenNoAuraEffect) {
  SpellDefinition def{};
  def.id = 1;
  SpellCastOutcome out{};
  SpellHitEffects::ApplyAuraFromDefinition(&def, 1, 2, 1, {}, nullptr, &out);
  EXPECT_FALSE(out.hasAuraApply);
}

TEST(SpellHitEffectsTests, ApplySpellEffectsFromDefinition_EnergizePower) {
  SpellDefinition def{};
  def.id = 28730u;
  SpellEffectRow row{};
  row.effectKind = kSpellEffectEnergize;
  row.basePoints = 15;
  def.effectRows.push_back(row);

  SpellCastOutcome out{};
  SpellHitEffects::ApplySpellEffectsFromDefinition(
      &def, 0x100ULL, 0x100ULL, 10, std::chrono::steady_clock::now(), nullptr, &out);
  EXPECT_EQ(out.power1Delta, 16);
  EXPECT_FALSE(out.hasAuraApply);
}

TEST(SpellHitEffectsTests, ApplySpellEffects_SumsMultipleHealthRows) {
  SpellDefinition def{};
  def.id = 133u;
  SpellEffectRow fire{};
  fire.effectKind = kSpellEffectSchoolDamage;
  fire.basePoints = 10;
  fire.dieSides = 0;
  SpellEffectRow ignite{};
  ignite.effectKind = kSpellEffectSchoolDamage;
  ignite.effectIndex = 1;
  ignite.basePoints = 4;
  ignite.dieSides = 0;
  def.effectRows.push_back(fire);
  def.effectRows.push_back(ignite);

  SpellCastOutcome out{};
  SpellHitEffects::ApplySpellEffectsFromDefinition(
      &def, 0x200ULL, 0x100ULL, 20, std::chrono::steady_clock::now(), nullptr, &out);
  ASSERT_TRUE(out.hasDirectHealthEffect);
  EXPECT_EQ(out.directHealthTargetGuid, 0x200ULL);
  EXPECT_EQ(out.directHealthDelta, -16);
}

TEST(SpellHitEffectsTests, ApplySpellEffects_PrefersEffectRowsOverImmediateHealthFallback) {
  SpellDefinition def{};
  def.id = 1u;
  def.immediateHealthEffectDelta = -99;
  SpellEffectRow heal{};
  heal.effectKind = kSpellEffectHeal;
  heal.basePoints = 5;
  def.effectRows.push_back(heal);

  SpellCastOutcome out{};
  SpellHitEffects::ApplySpellEffectsFromDefinition(
      &def, 0x50ULL, 0x50ULL, 10, std::chrono::steady_clock::now(), nullptr, &out);
  ASSERT_TRUE(out.hasDirectHealthEffect);
  EXPECT_EQ(out.directHealthDelta, 6);
}

TEST(SpellHitEffectsTests, ApplySpellEffects_HealthLeechCountsAsDamage) {
  SpellDefinition def{};
  SpellEffectRow leech{};
  leech.effectKind = kSpellEffectHealthLeech;
  leech.basePoints = 8;
  def.effectRows.push_back(leech);

  SpellCastOutcome out{};
  SpellHitEffects::ApplySpellEffectsFromDefinition(
      &def, 0x30ULL, 0x10ULL, 1, std::chrono::steady_clock::now(), nullptr, &out);
  ASSERT_TRUE(out.hasDirectHealthEffect);
  EXPECT_EQ(out.directHealthDelta, -9);
}

TEST(SpellHitEffectsTests, ApplyAuraFromDefinition_PicksPeriodicRowFromAuraEffects) {
  SpellDefinition def{};
  def.id = 774u;
  def.hasAuraEffect = true;
  def.auraEffectType = 29u;
  def.auraEffectIndex = 0;
  SpellAuraEffectRow stat{};
  stat.effectIndex = 0;
  stat.auraType = kSpellAuraModStat;
  stat.basePoints = 5;
  SpellAuraEffectRow hot{};
  hot.effectIndex = 1;
  hot.auraType = kSpellAuraPeriodicHeal;
  hot.basePoints = -4;
  hot.periodMs = 3000;
  def.auraEffects.push_back(stat);
  def.auraEffects.push_back(hot);

  SpellCastOutcome out{};
  SpellHitEffects::ApplyAuraFromDefinition(&def, 0x50ULL, 0x40ULL, 80,
                                           std::chrono::steady_clock::now(), nullptr, &out);
  ASSERT_TRUE(out.hasAuraApply);
  EXPECT_EQ(out.auraEffectType, kSpellAuraPeriodicHeal);
  EXPECT_EQ(out.auraEffectIndex, 1u);
  EXPECT_EQ(out.auraPeriodicPeriodMs, 3000u);
  EXPECT_EQ(out.auraPeriodicHealthDeltaPerTick, 4);
}
