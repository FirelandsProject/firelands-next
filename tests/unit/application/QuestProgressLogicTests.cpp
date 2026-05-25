#include <gtest/gtest.h>
#include <application/logic/QuestProgressLogic.h>
#include <application/world/PlayerQuestProgressStore.h>
#include <shared/game/PlayerClass.h>
#include <optional>

using namespace Firelands;

namespace {

class MockQuestRepo final : public IQuestGossipRepository {
public:
  MockQuestRepo(std::vector<QuestGossipSummary> starters,
                std::vector<QuestGossipSummary> enders = {})
      : m_starters(std::move(starters)), m_enders(std::move(enders)) {}

  std::vector<QuestGossipSummary>
  GetStarterQuestsForCreature(uint32_t /*creatureEntry*/) const override {
    return m_starters;
  }

  std::vector<QuestGossipSummary>
  GetEnderQuestsForCreature(uint32_t /*creatureEntry*/) const override {
    return m_enders;
  }

  std::optional<QuestGossipSummary>
  TryGetQuestTemplate(uint32_t questId) const override {
    for (QuestGossipSummary const &q : m_starters) {
      if (q.questId == questId)
        return q;
    }
    for (QuestGossipSummary const &q : m_enders) {
      if (q.questId == questId)
        return q;
    }
    return std::nullopt;
  }

private:
  std::vector<QuestGossipSummary> m_starters;
  std::vector<QuestGossipSummary> m_enders;
};

QuestGossipSummary MakeQuest(uint32_t id, uint32_t classes = 0, uint32_t races = 0) {
  QuestGossipSummary q{};
  q.questId = id;
  q.title = "Test Quest";
  q.questLevel = 5;
  q.flags = 0;
  q.allowableClasses = classes;
  q.allowableRaces = races;
  return q;
}

} // namespace

TEST(QuestProgressLogicTests, FindStarterQuestMatchesEntry) {
  MockQuestRepo repo({MakeQuest(10), MakeQuest(20)});
  auto const found = FindStarterQuestForCreature(&repo, 1, 20);
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->questId, 20u);
  EXPECT_FALSE(FindStarterQuestForCreature(&repo, 1, 99).has_value());
}

TEST(QuestProgressLogicTests, FindEnderQuestMatchesEntry) {
  MockQuestRepo repo({}, {MakeQuest(24764)});
  auto const found = FindEnderQuestForCreature(&repo, 38243, 24764);
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->questId, 24764u);
}

TEST(QuestProgressLogicTests, AcceptRejectedWhenAlreadyRewarded) {
  PlayerQuestProgressStore progress;
  progress.SetQuestRewarded(10);
  auto const result =
      EvaluateQuestAccept(MakeQuest(10), progress, 1, 1);
  EXPECT_EQ(result, QuestAcceptResult::AlreadyRewarded);
}

TEST(QuestProgressLogicTests, AcceptRejectedWhenAlreadyActive) {
  PlayerQuestProgressStore progress;
  progress.SetQuestStatus(10, QuestStatus::Incomplete);
  EXPECT_EQ(EvaluateQuestAccept(MakeQuest(10), progress, 1, 1),
            QuestAcceptResult::AlreadyOnQuest);
}

TEST(QuestProgressLogicTests, InvalidReasonMapsAlreadyOnQuest) {
  EXPECT_EQ(QuestAcceptResultToInvalidReason(QuestAcceptResult::AlreadyOnQuest), 13u);
}

TEST(QuestProgressLogicTests, DialogStatusRewardOnlyOnEnderWhenComplete) {
  PlayerQuestProgressStore progress;
  progress.SetQuestStatus(24764, QuestStatus::Complete);
  constexpr uint8_t kTrollDruidClass = ToClassId(PlayerClass::Druid);
  constexpr uint8_t kTrollRaceId = 8u;
  MockQuestRepo repo({MakeQuest(24764, 1024, 946)}, {MakeQuest(24764, 1024, 946)});
  EXPECT_EQ(ResolveQuestGiverDialogStatusForPlayer(
                &repo, 37951, kTrollDruidClass, kTrollRaceId, progress),
            QuestGiverDialogStatus::None);
  EXPECT_EQ(ResolveQuestGiverDialogStatusForPlayer(
                &repo, 38243, kTrollDruidClass, kTrollRaceId, progress),
            QuestGiverDialogStatus::Reward);
}

TEST(QuestProgressLogicTests, StarterGossipHidesCompleteQuestTurnIn) {
  PlayerQuestProgressStore progress;
  progress.SetQuestStatus(10, QuestStatus::Complete);
  MockQuestRepo repo({MakeQuest(10)});
  EXPECT_EQ(ResolveStarterQuestGossipIconForPlayer(MakeQuest(10), progress),
            QuestGossipIcon::None);
  auto const items = BuildStarterGossipQuestItemsForPlayer({MakeQuest(10)}, progress);
  EXPECT_TRUE(items.empty());
}

TEST(QuestProgressLogicTests, EnderGossipShowsQuestionWhenComplete) {
  PlayerQuestProgressStore progress;
  progress.SetQuestStatus(10, QuestStatus::Complete);
  auto const items = BuildEnderGossipQuestItemsForPlayer({MakeQuest(10)}, progress);
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0].questIcon, static_cast<uint8_t>(QuestGossipIcon::Complete));
}
