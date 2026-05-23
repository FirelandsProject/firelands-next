#include <gtest/gtest.h>
#include <shared/game/SpellPowerResolve.h>

using namespace Firelands;

TEST(SpellPowerResolveTests, ManaUsesPercentOfMaxPower) {
  SpellPowerDbcRow row{};
  row.costPercent = 9;
  EXPECT_EQ(ResolveSpellPowerCost(row, 0u, 10, 1, 1000u), 90u);
}

TEST(SpellPowerResolveTests, ManaUsesFloatPercentWhenIntegerPercentZero) {
  SpellPowerDbcRow row{};
  row.costPercentFloat = 13.f;
  EXPECT_EQ(ResolveSpellPowerCost(row, 0u, 20, 1, 500u), 65u);
}

TEST(SpellPowerResolveTests, EnergyUsesFlatCost) {
  SpellPowerDbcRow row{};
  row.flatCost = 45u;
  EXPECT_EQ(ResolveSpellPowerCost(row, 3u, 10, 1, 100u), 45u);
}

TEST(SpellPowerResolveTests, RageDividesFlatCostByTen) {
  SpellPowerDbcRow row{};
  row.flatCost = 300u;
  EXPECT_EQ(ResolveSpellPowerCost(row, 1u, 80, 1, 1000u), 30u);
}

TEST(SpellPowerResolveTests, AddsCostPerLevelAboveSpellLevel) {
  SpellPowerDbcRow row{};
  row.flatCost = 10u;
  row.costPerLevel = 5u;
  EXPECT_EQ(ResolveSpellPowerCost(row, 3u, 10, 5, 100u), 35u);
}
