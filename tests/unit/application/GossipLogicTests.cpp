#include <application/logic/GossipLogic.h>
#include <application/logic/QuestProgressLogic.h>
#include <application/world/PlayerQuestProgressStore.h>
#include <domain/models/QuestGossip.h>
#include <shared/game/UnitNpcFlags.h>
#include <gtest/gtest.h>

namespace Firelands {

TEST(GossipLogicTests, ResolveGossipMenuId_UsesTemplateWhenSet) {
  EXPECT_EQ(ResolveGossipMenuIdForTemplate(435), 435u);
  EXPECT_EQ(ResolveGossipMenuIdForTemplate(0), kDefaultNpcGossipMenuId);
}

TEST(GossipLogicTests, CreatureUsesGossipMenuDialog_DistinguishesQuestHubFromTrainer) {
  // Jinthala (37951): quest giver only, no gossip menu.
  EXPECT_FALSE(CreatureUsesGossipMenuDialog(0, kUnitNpcFlagQuestGiver));
  // Zentabra (38243): gossip menu 10984.
  EXPECT_TRUE(CreatureUsesGossipMenuDialog(10984, kUnitNpcFlagGossip));
  EXPECT_TRUE(CreatureUsesGossipMenuDialog(0, kUnitNpcFlagGossip));
}

TEST(GossipLogicTests, ResolveGossipNpcTextId_FallsBackWhenMenuTextMissing) {
  EXPECT_EQ(ResolveGossipNpcTextId(15272), 15272u);
  EXPECT_EQ(ResolveGossipNpcTextId(0), 1u);
}

TEST(GossipLogicTests, FilterGossipOptionsByNpcFlags_MatchesRoleFlags) {
  GossipMenuItem vendor;
  vendor.optionIndex = 0;
  vendor.optionNpcflag = 128;

  GossipMenuItem gossip;
  gossip.optionIndex = 1;
  gossip.optionNpcflag = 1;

  GossipMenuItem ungated;
  ungated.optionIndex = 2;
  ungated.optionNpcflag = 0;

  std::vector<GossipMenuItem> items = {vendor, gossip, ungated};
  auto filtered = FilterGossipOptionsByNpcFlags(std::move(items), 128);

  ASSERT_EQ(filtered.size(), 2u);
  EXPECT_EQ(filtered[0].optionIndex, 0u);
  EXPECT_EQ(filtered[1].optionIndex, 2u);
}

TEST(GossipLogicTests, FilterGossipOptionsByNpcFlags_ZeroFlagsKeepsAll) {
  GossipMenuItem a;
  a.optionNpcflag = 128;
  std::vector<GossipMenuItem> items = {a};
  auto filtered = FilterGossipOptionsByNpcFlags(std::move(items), 0);
  EXPECT_EQ(filtered.size(), 1u);
}

TEST(GossipLogicTests, ShouldSendGossipMenu_QuestLinesOnly) {
  EXPECT_TRUE(ShouldSendGossipMenu(0, false, 1));
  EXPECT_FALSE(ShouldSendGossipMenu(0, false, 0));
  EXPECT_TRUE(ShouldSendGossipMenu(0, true, 0));
  EXPECT_TRUE(ShouldSendGossipMenu(1, false, 0));
}

TEST(GossipLogicTests, FindGossipMenuItem_ByOptionIndex) {
  GossipMenuItem a;
  a.optionIndex = 3;
  std::vector<GossipMenuItem> items = {a};
  EXPECT_NE(FindGossipMenuItem(items, 3), nullptr);
  EXPECT_EQ(FindGossipMenuItem(items, 9), nullptr);
}

TEST(GossipLogicTests, BuildGossipQuestItems_MapsSummaryToWireFields) {
  QuestGossipSummary summary;
  summary.questId = 42;
  summary.title = "Kill rats";
  summary.questLevel = 3;
  summary.flags = kQuestFlagAutoComplete;

  PlayerQuestProgressStore progress;
  auto items = BuildStarterGossipQuestItemsForPlayer({summary}, progress, 1, 1, 80);
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0].questId, 42u);
  EXPECT_EQ(items[0].questTitle, "Kill rats");
  EXPECT_EQ(items[0].questLevel, 3);
  EXPECT_EQ(items[0].questFlags, kQuestFlagAutoComplete);
  EXPECT_FALSE(items[0].blueQuestionMark);
  EXPECT_EQ(items[0].questIcon,
            static_cast<uint8_t>(QuestGossipIcon::Available));
}

} // namespace Firelands
