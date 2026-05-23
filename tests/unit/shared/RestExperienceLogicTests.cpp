#include <gtest/gtest.h>
#include <shared/game/RestExperienceLogic.h>

using namespace Firelands::RestExperienceLogic;

TEST(RestExperienceLogicTest, ConsumeForKillCapsBonusAtBaseXp) {
  RestConsumeResult const r = ConsumeForKill(500.f, 120u, 400u);
  EXPECT_EQ(r.restedBonus, 120u);
  EXPECT_FLOAT_EQ(r.restBonusAfter, 380.f);
}

TEST(RestExperienceLogicTest, RestStateClearsWhenPoolNearlyEmpty) {
  EXPECT_EQ(RestStateForBonus(0.f), RestStateWire::NotRafLinked);
  EXPECT_EQ(RestStateForBonus(1.f), RestStateWire::NotRafLinked);
  EXPECT_EQ(RestStateForBonus(11.f), RestStateWire::Rested);
}

TEST(RestExperienceLogicTest, RestBonusCapUsesNextLevelXp) {
  EXPECT_FLOAT_EQ(RestBonusCap(400u), 300.f);
  EXPECT_FLOAT_EQ(ClampRestBonus(9999.f, 400u), 300.f);
}
