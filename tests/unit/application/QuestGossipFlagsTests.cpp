#include <gtest/gtest.h>
#include <application/logic/QuestProgressLogic.h>
#include <domain/models/QuestGossip.h>

using namespace Firelands;

TEST(QuestGossipFlagsTests, RiseOfTheDarkspearUsesAutoAccept) {
  constexpr uint32_t kRiseOfTheDarkspearFlags = 524288u;
  EXPECT_TRUE(QuestHasAutoAcceptFlag(kRiseOfTheDarkspearFlags));
  EXPECT_FALSE(QuestHasAutoCompleteFlag(kRiseOfTheDarkspearFlags));
  EXPECT_EQ(QuestFlagsForDetailsPacket(kRiseOfTheDarkspearFlags), 0u);
}

TEST(QuestGossipFlagsTests, RiseOfTheDarkspearCompletesOnMeetNpc) {
  QuestGossipSummary summary;
  summary.flags = 524288u;
  EXPECT_FALSE(summary.HasTrackableObjectives());
  EXPECT_TRUE(QuestCompletesOnMeetNpc(summary, 38243));
}

TEST(QuestGossipFlagsTests, KillQuestHasTrackableObjectives) {
  QuestGossipSummary summary;
  summary.requiredNpcOrGo[0] = 12345;
  summary.requiredNpcOrGoCount[0] = 8;
  EXPECT_TRUE(summary.HasTrackableObjectives());
  EXPECT_FALSE(QuestCompletesOnMeetNpc(summary, 38243));
}
