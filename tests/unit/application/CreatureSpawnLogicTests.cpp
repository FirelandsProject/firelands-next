#include <application/logic/CreatureSpawnLogic.h>
#include <gtest/gtest.h>
#include <random>

namespace Firelands {

TEST(CreatureSpawnLogicTests, PickCreatureLevelInclusive_StaysInRange) {
  std::mt19937 rng(424242u);
  for (int i = 0; i < 80; ++i) {
    uint8 const v = PickCreatureLevelInclusive(3, 7, rng);
    EXPECT_GE(v, 3u);
    EXPECT_LE(v, 7u);
  }
}

TEST(CreatureSpawnLogicTests, PickCreatureLevelInclusive_SwapsInvertedBounds) {
  std::mt19937 rng(1u);
  uint8 const v = PickCreatureLevelInclusive(10, 5, rng);
  EXPECT_GE(v, 5u);
  EXPECT_LE(v, 10u);
}

TEST(CreatureSpawnLogicTests, NormalizeCreatureUnitClass_ZeroBecomesWarrior) {
  EXPECT_EQ(NormalizeCreatureUnitClass(0), 1u);
  EXPECT_EQ(NormalizeCreatureUnitClass(8), 8u);
}

TEST(CreatureSpawnLogicTests, FallbackCreatureBaseHealth_ScalesWithLevel) {
  EXPECT_GE(FallbackCreatureBaseHealth(10), FallbackCreatureBaseHealth(5));
}

} // namespace Firelands
