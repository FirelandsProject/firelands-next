#include <application/logic/QuestGiverLogic.h>
#include <application/world/PlayerQuestProgressStore.h>
#include <domain/models/QuestGiverStatus.h>
#include <domain/models/QuestGossip.h>
#include <domain/repositories/IQuestGossipRepository.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <optional>
#include <vector>

namespace Firelands {

namespace {

class MockQuestGossipRepository : public IQuestGossipRepository {
public:
  MOCK_METHOD(std::vector<QuestGossipSummary>, GetStarterQuestsForCreature,
              (uint32_t creatureEntry), (const, override));
  MOCK_METHOD(std::vector<QuestGossipSummary>, GetEnderQuestsForCreature,
              (uint32_t creatureEntry), (const, override));
  MOCK_METHOD(std::optional<QuestGossipSummary>, TryGetQuestTemplate, (uint32_t),
              (const, override));
  MOCK_METHOD(std::vector<uint32_t>, GetInvolvedCreatureEntriesForQuest, (uint32_t),
              (const, override));
};

} // namespace

TEST(QuestGiverLogicTests, EffectiveNpcFlags_AddsQuestGiverWhenStartersExist) {
  EXPECT_EQ(EffectiveUnitNpcFlagsForCreature(0, false), 0u);
  EXPECT_EQ(EffectiveUnitNpcFlagsForCreature(0, true), kUnitNpcFlagQuestGiver);
  EXPECT_EQ(EffectiveUnitNpcFlagsForCreature(
                kUnitNpcFlagGossip | kUnitNpcFlagFlightMaster, true),
            kUnitNpcFlagGossip | kUnitNpcFlagQuestGiver);
}

TEST(QuestGiverLogicTests, ResolveDialogStatus_UsesRepository) {
  PlayerQuestProgressStore progress;
  MockQuestGossipRepository repo;
  EXPECT_EQ(ResolveQuestGiverDialogStatus(&repo, 100, 11, 8, 80, progress),
            QuestGiverDialogStatus::None);

  QuestGossipSummary summary;
  summary.questId = 1;
  summary.allowableClasses = 1024; // Druid only
  summary.allowableRaces = 128;    // Troll only
  EXPECT_CALL(repo, GetStarterQuestsForCreature(38243))
      .WillRepeatedly(testing::Return(std::vector<QuestGossipSummary>{summary}));
  EXPECT_CALL(repo, GetEnderQuestsForCreature(38243))
      .WillRepeatedly(testing::Return(std::vector<QuestGossipSummary>{}));
  EXPECT_EQ(ResolveQuestGiverDialogStatus(&repo, 38243, 11, 8, 80, progress),
            QuestGiverDialogStatus::Available);
  EXPECT_EQ(ResolveQuestGiverDialogStatus(&repo, 38243, 1, 8, 80, progress),
            QuestGiverDialogStatus::None);
  EXPECT_EQ(static_cast<uint32_t>(QuestGiverDialogStatus::Available), 0x100u);
}

} // namespace Firelands
