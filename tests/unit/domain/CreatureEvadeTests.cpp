#include <gtest/gtest.h>
#include <domain/world/Creature.h>

using namespace Firelands;

TEST(CreatureEvadeTests, EvadeRegenHealsTowardMax) {
  Creature creature(1, 100, 200, 1000, 10);
  creature.SetEvading(true);
  creature.ApplyHealthDelta(-500);
  EXPECT_EQ(creature.GetLiveHealth(), 500u);
  creature.TickEvadeHealthRegen(std::chrono::milliseconds{2000});
  EXPECT_GT(creature.GetLiveHealth(), 500u);
}

TEST(CreatureEvadeTests, RestoreHealthToFullAtHome) {
  Creature creature(1, 100, 200, 1000, 10);
  creature.SetEvading(true);
  creature.ApplyHealthDelta(-800);
  creature.RestoreHealthToFull();
  creature.SetEvading(false);
  EXPECT_EQ(creature.GetLiveHealth(), creature.GetLiveMaxHealth());
}
