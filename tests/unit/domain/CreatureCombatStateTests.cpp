#include <gtest/gtest.h>
#include <domain/world/Creature.h>
#include <shared/game/UnitFieldFlags.h>

using namespace Firelands;

TEST(CreatureCombatStateTests, MarkInCombatSetsUnitFlag) {
  Creature creature(1, 100, 200, 1000, 10);
  EXPECT_FALSE(creature.IsInCombat());
  creature.MarkInCombat();
  EXPECT_TRUE(creature.IsInCombat());
  EXPECT_NE(creature.GetUnitFieldFlags() & kUnitFlagInCombat, 0u);
}

TEST(CreatureCombatStateTests, MarkDeadAndLootableClearsCombatAndSetsDynamicFlags) {
  Creature creature(1, 100, 200, 1000, 10);
  creature.MarkInCombat();
  creature.ApplyHealthDelta(-static_cast<int32>(creature.GetLiveMaxHealth()));
  creature.MarkDeadAndLootable();
  EXPECT_TRUE(creature.IsDead());
  EXPECT_FALSE(creature.IsInCombat());
  EXPECT_NE(creature.GetUnitDynamicFlags() & kUnitDynflagLootable, 0u);
  EXPECT_NE(creature.GetUnitDynamicFlags() & kUnitDynflagTappedByPlayer, 0u);
}
