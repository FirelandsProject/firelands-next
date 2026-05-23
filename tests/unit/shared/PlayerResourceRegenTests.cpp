#include <gtest/gtest.h>
#include <shared/game/PlayerPowerType.h>
#include <shared/game/PlayerResourceRegen.h>

using namespace Firelands;

TEST(PlayerResourceRegenTests, EnergyRegeneratesInCombat) {
  ResourceRegenDelta const delta = ComputeResourceRegenDelta(
      PlayerPowerType::Energy, 85, 0, 100, 100, 50, 100, false,
      std::chrono::milliseconds{2000});
  EXPECT_EQ(delta.health, 0);
  EXPECT_EQ(delta.power1, 20);
}

TEST(PlayerResourceRegenTests, RageDecaysOutOfCombat) {
  ResourceRegenDelta const delta = ComputeResourceRegenDelta(
      PlayerPowerType::Rage, 85, 0, 100, 100, 500, 1000, true,
      std::chrono::milliseconds{2000});
  EXPECT_LT(delta.power1, 0);
}

TEST(PlayerResourceRegenTests, ManaUsesSpiritOutOfCombat) {
  ResourceRegenDelta const lowSpirit = ComputeResourceRegenDelta(
      PlayerPowerType::Mana, 85, 50, 100, 100, 100, 1000, true,
      std::chrono::milliseconds{5000});
  ResourceRegenDelta const highSpirit = ComputeResourceRegenDelta(
      PlayerPowerType::Mana, 85, 200, 100, 100, 100, 1000, true,
      std::chrono::milliseconds{5000});
  EXPECT_GT(highSpirit.power1, lowSpirit.power1);
}

TEST(PlayerResourceRegenTests, TrollRegenPctBoostsHealthOutOfCombat) {
  ResourceRegenModifiers troll{};
  troll.healthRegenPct = 10;
  ResourceRegenDelta const base = ComputeResourceRegenDelta(
      PlayerPowerType::Mana, 85, 100, 50, 1000, 100, 1000, true,
      std::chrono::milliseconds{2000}, {});
  ResourceRegenDelta const boosted = ComputeResourceRegenDelta(
      PlayerPowerType::Mana, 85, 100, 50, 1000, 100, 1000, true,
      std::chrono::milliseconds{2000}, troll);
  EXPECT_GT(boosted.health, base.health);
}

TEST(PlayerResourceRegenTests, TrollRegenAllowsPartialHealthInCombat) {
  ResourceRegenModifiers troll{};
  troll.healthRegenPct = 10;
  troll.healthRegenDuringCombatPct = 10;
  ResourceRegenDelta const inCombat = ComputeResourceRegenDelta(
      PlayerPowerType::Mana, 85, 100, 50, 1000, 100, 1000, false,
      std::chrono::milliseconds{2000}, troll);
  EXPECT_GT(inCombat.health, 0);
}

TEST(PlayerResourceRegenTests, HealthRegenOnlyOutOfCombat) {
  ResourceRegenDelta const inCombat = ComputeResourceRegenDelta(
      PlayerPowerType::Mana, 85, 100, 50, 100, 100, 1000, false,
      std::chrono::milliseconds{2000});
  ResourceRegenDelta const outOfCombat = ComputeResourceRegenDelta(
      PlayerPowerType::Mana, 85, 100, 50, 100, 100, 1000, true,
      std::chrono::milliseconds{2000});
  EXPECT_EQ(inCombat.health, 0);
  EXPECT_GT(outOfCombat.health, 0);
}
