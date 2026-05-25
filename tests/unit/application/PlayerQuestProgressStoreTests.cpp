#include <gtest/gtest.h>
#include <application/world/PlayerQuestProgressStore.h>

using namespace Firelands;

TEST(PlayerQuestProgressStoreTests, HasFreeSlotOnFreshStore) {
  PlayerQuestProgressStore store;
  EXPECT_TRUE(store.HasFreeQuestLogSlot());
}

TEST(PlayerQuestProgressStoreTests, AssignsQuestLogSlotOnAccept) {
  PlayerQuestProgressStore store;
  auto const slot = store.TryAssignQuestLogSlot(100);
  ASSERT_TRUE(slot.has_value());
  EXPECT_EQ(*slot, 0u);
  EXPECT_EQ(store.FindQuestLogSlot(100), slot);
  EXPECT_EQ(store.GetQuestLogSlotQuestId(*slot), 100u);
}

TEST(PlayerQuestProgressStoreTests, ClearsSlotWhenRewarded) {
  PlayerQuestProgressStore store;
  store.SetQuestStatus(42, QuestStatus::Incomplete);
  ASSERT_TRUE(store.FindQuestLogSlot(42).has_value());
  store.SetQuestRewarded(42);
  EXPECT_FALSE(store.FindQuestLogSlot(42).has_value());
}

TEST(PlayerQuestProgressStoreTests, ExportSnapshotMatchesActiveAndRewarded) {
  PlayerQuestProgressStore store;
  store.SetQuestStatus(10, QuestStatus::Incomplete);
  store.SetQuestRewarded(99);

  auto const snap = store.ExportSnapshot();
  EXPECT_EQ(snap.activeQuests.size(), 1u);
  EXPECT_EQ(snap.activeQuests.at(10), QuestStatus::Incomplete);
  EXPECT_EQ(snap.rewardedQuests.count(99), 1u);
  EXPECT_EQ(snap.rewardedQuests.count(10), 0u);
}

TEST(PlayerQuestProgressStoreTests, RebuildsSlotsFromLoadedSnapshot) {
  PlayerQuestProgressStore store;
  PlayerQuestProgressSnapshot snap;
  snap.activeQuests.emplace(10, QuestStatus::Incomplete);
  snap.activeQuests.emplace(20, QuestStatus::Complete);
  store.LoadSnapshot(std::move(snap));
  auto const slot10 = store.FindQuestLogSlot(10);
  auto const slot20 = store.FindQuestLogSlot(20);
  ASSERT_TRUE(slot10.has_value());
  ASSERT_TRUE(slot20.has_value());
  EXPECT_NE(*slot10, *slot20);
}
