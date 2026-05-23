#include <application/world/PhaseAreaCatalog.h>
#include <application/world/PhaseConditionEvaluator.h>
#include <application/world/PlayerQuestProgressStore.h>
#include <gtest/gtest.h>

namespace Firelands {
namespace {

TEST(PhaseAreaCatalogTests, DirectAreaLookupWithoutConditions) {
  PhaseAreaCatalog catalog;
  catalog.Load({{4756u, {{169u, {}}}}});
  PlayerQuestProgressStore player;
  auto const phases = catalog.ResolveForArea(4756, player);
  ASSERT_EQ(phases.size(), 1u);
  EXPECT_EQ(phases[0], 169u);
}

TEST(PhaseAreaCatalogTests, ParentAreaInheritsPhase) {
  PhaseAreaCatalog catalog;
  catalog.Load({{4714u, {{105u, {}}}}});

  auto const parentOf = [](uint32 area) -> uint32 {
    if (area == 4756u)
      return 4714u;
    return 0u;
  };

  PlayerQuestProgressStore player;
  auto const phases = catalog.ResolveForArea(4756, player, parentOf);
  ASSERT_EQ(phases.size(), 1u);
  EXPECT_EQ(phases[0], 105u);
}

TEST(PhaseAreaCatalogTests, FiltersPhaseByQuestRewardCondition) {
  PhaseCondition rewarded;
  rewarded.type = PhaseConditionType::QuestRewarded;
  rewarded.value1 = 28598u;

  PhaseCondition beforeQuest;
  beforeQuest.elseGroup = 1;
  beforeQuest.type = PhaseConditionType::QuestRewarded;
  beforeQuest.value1 = 28598u;
  beforeQuest.negative = true;

  PhaseAreaCatalog catalog;
  catalog.Load({{5140u, {{169u, {rewarded}}, {361u, {beforeQuest}}}}});

  PlayerQuestProgressStore fresh;
  auto const before = catalog.ResolveForArea(5140, fresh);
  ASSERT_EQ(before.size(), 1u);
  EXPECT_EQ(before[0], 361u);

  PlayerQuestProgressStore progressed;
  progressed.SetQuestRewarded(28598u);
  auto const after = catalog.ResolveForArea(5140, progressed);
  ASSERT_EQ(after.size(), 1u);
  EXPECT_EQ(after[0], 169u);
}

} // namespace
} // namespace Firelands
