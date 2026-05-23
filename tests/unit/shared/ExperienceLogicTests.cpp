#include <gtest/gtest.h>
#include <shared/game/ExperienceLogic.h>

using namespace Firelands;
using namespace Firelands::ExperienceLogic;

TEST(ExperienceLogicTest, GrayLevelMatchesreferenceBands) {
  EXPECT_EQ(GrayLevelForPlayer(5), 0u);
  EXPECT_EQ(GrayLevelForPlayer(10), 5u);
  EXPECT_EQ(GrayLevelForPlayer(40), 30u);
  EXPECT_EQ(GrayLevelForPlayer(60), 45u);
  EXPECT_EQ(GrayLevelForPlayer(85), 65u);
}

TEST(ExperienceLogicTest, NoKillXpWhenCreatureIsGray) {
  ExperienceRates rates{};
  rates.kill = 1.0f;
  EXPECT_EQ(CalculateKillExperience(40, 30, 1.0f, rates), 0u);
}

TEST(ExperienceLogicTest, KillRateScalesAward) {
  ExperienceRates rates{};
  rates.kill = 2.0f;
  uint32_t const base =
      CalculateKillExperience(10, 10, 1.0f, ExperienceRates{.kill = 1.0f});
  uint32_t const doubled = CalculateKillExperience(10, 10, 1.0f, rates);
  EXPECT_GT(base, 0u);
  EXPECT_EQ(doubled, base * 2u);
}

TEST(ExperienceLogicTest, ApplyExperienceGainLevelsUpAndCarriesRemainder) {
  auto xpToNext = [](uint8_t level) -> uint32_t {
    if (level == 1)
      return 100u;
    if (level == 2)
      return 200u;
    return 0u;
  };

  ExperienceGainResult const r =
      ApplyExperienceGain(1, 80, 150, 85, xpToNext);
  EXPECT_EQ(r.level, 2u);
  EXPECT_EQ(r.xp, 130u);
  EXPECT_EQ(r.levelsGained, 1u);
}

TEST(ExperienceLogicTest, MaxLevelClearsXp) {
  ExperienceGainResult const r = ApplyExperienceGain(85, 10, 500, 85, [](uint8_t) {
    return 400u;
  });
  EXPECT_EQ(r.level, 85u);
  EXPECT_EQ(r.xp, 0u);
  EXPECT_EQ(r.levelsGained, 0u);
}
