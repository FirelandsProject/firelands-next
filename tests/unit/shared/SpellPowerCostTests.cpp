#include <gtest/gtest.h>
#include <shared/game/SpellPowerCost.h>

using namespace Firelands;

TEST(SpellPowerCostTests, AssignSpellPowerFieldsFromDbc_UsesField42) {
  uint32 powerType = 0;
  uint32 spellPowerId = 0;
  AssignSpellPowerFieldsFromDbc(0u, 33u, powerType, spellPowerId);
  EXPECT_EQ(powerType, 0u);
  EXPECT_EQ(spellPowerId, 33u);
}

TEST(SpellPowerCostTests, AssignSpellPowerFieldsFromDbc_FallbackField14WhenSpellPowerRow) {
  uint32 powerType = 0;
  uint32 spellPowerId = 0;
  AssignSpellPowerFieldsFromDbc(140u, 0u, powerType, spellPowerId);
  EXPECT_EQ(powerType, 0u);
  EXPECT_EQ(spellPowerId, 140u);
}

TEST(SpellPowerCostTests, AssignSpellPowerFieldsFromDbc_KeepsPowerTypeEnum) {
  uint32 powerType = 0;
  uint32 spellPowerId = 0;
  AssignSpellPowerFieldsFromDbc(3u, 140u, powerType, spellPowerId);
  EXPECT_EQ(powerType, 3u);
  EXPECT_EQ(spellPowerId, 140u);
}

TEST(SpellPowerCostTests, MaxPower1ForSpellPercentCost_FallbackWhenLiveMaxIsOne) {
  EXPECT_EQ(MaxPower1ForSpellPercentCost(0u, 10, 1u), 200u);
  EXPECT_EQ(MaxPower1ForSpellPercentCost(3u, 10, 1u), 1u);
}

TEST(SpellPowerCostTests, EffectiveSpellPowerTypeForCast_UsesCasterWhenField14IsRowId) {
  EXPECT_EQ(EffectiveSpellPowerTypeForCast(140u, 0u), 0u);
  EXPECT_EQ(EffectiveSpellPowerTypeForCast(3u, 3u), 3u);
}
