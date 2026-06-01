#include <gtest/gtest.h>
#include <shared/game/SpellCastHaste.h>

using namespace Firelands;

TEST(SpellCastHasteTests, NoChangeWhenMultiplierIsOne) {
  EXPECT_EQ(ApplyCastHasteMultiplierToCastTimeMs(2000u, 1.f), 2000u);
}

TEST(SpellCastHasteTests, BerserkingScaleTwentyOnePercent) {
  EXPECT_EQ(ApplyCastHasteMultiplierToCastTimeMs(3200u, 1.21f), 2644u);
}
