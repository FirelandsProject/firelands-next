#include <gtest/gtest.h>
#include <shared/game/PlayerFactionTeam.h>

using namespace Firelands;

TEST(PlayerRaceFactionTemplateTests, SafeFallbackAllianceBaseline) {
  EXPECT_EQ(SafePlayerFactionTemplateWithoutChrRaces(1), 1u);
  EXPECT_EQ(SafePlayerFactionTemplateWithoutChrRaces(3), 1u);
  EXPECT_EQ(SafePlayerFactionTemplateWithoutChrRaces(4), 1u);
  EXPECT_EQ(SafePlayerFactionTemplateWithoutChrRaces(7), 1u);
  EXPECT_EQ(SafePlayerFactionTemplateWithoutChrRaces(11), 1u);
  EXPECT_EQ(SafePlayerFactionTemplateWithoutChrRaces(22), 1u);
}

TEST(PlayerRaceFactionTemplateTests, SafeFallbackHordeBaseline) {
  EXPECT_EQ(SafePlayerFactionTemplateWithoutChrRaces(2), 2u);
  EXPECT_EQ(SafePlayerFactionTemplateWithoutChrRaces(5), 2u);
  EXPECT_EQ(SafePlayerFactionTemplateWithoutChrRaces(6), 2u);
  EXPECT_EQ(SafePlayerFactionTemplateWithoutChrRaces(8), 2u);
  EXPECT_EQ(SafePlayerFactionTemplateWithoutChrRaces(9), 2u);
  EXPECT_EQ(SafePlayerFactionTemplateWithoutChrRaces(10), 2u);
}

TEST(PlayerRaceFactionTemplateTests, SafeFallbackUnknownUsesAlliance) {
  EXPECT_EQ(SafePlayerFactionTemplateWithoutChrRaces(0), 1u);
  EXPECT_EQ(SafePlayerFactionTemplateWithoutChrRaces(99), 1u);
}
