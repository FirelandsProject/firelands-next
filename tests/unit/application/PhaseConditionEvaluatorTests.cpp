#include <application/world/PhaseConditionEvaluator.h>

#include <domain/models/PhaseCondition.h>
#include <domain/models/QuestProgress.h>
#include <domain/ports/IPlayerQuestProgress.h>
#include <shared/Common.h>
#include <gtest/gtest.h>

#include <unordered_map>
#include <unordered_set>

namespace Firelands {
namespace {

class FakeQuestProgress : public IPlayerQuestProgress {
public:
  QuestStatus GetQuestStatus(uint32 questId) const override {
    if (auto const it = m_status.find(questId); it != m_status.end())
      return it->second;
    return QuestStatus::None;
  }

  bool IsQuestRewarded(uint32 questId) const override {
    return m_rewarded.count(questId) != 0;
  }

  bool HasAuraSpell(uint32 spellId) const override {
    return m_auras.count(spellId) != 0;
  }

  void SetStatus(uint32 questId, QuestStatus status) { m_status[questId] = status; }
  void SetRewarded(uint32 questId) { m_rewarded.insert(questId); }
  void SetAura(uint32 spellId) { m_auras.insert(spellId); }

private:
  std::unordered_map<uint32, QuestStatus> m_status;
  std::unordered_set<uint32> m_rewarded;
  std::unordered_set<uint32> m_auras;
};

TEST(PhaseConditionEvaluatorTests, EmptyConditionsAlwaysPass) {
  FakeQuestProgress player;
  EXPECT_TRUE(EvaluatePhaseConditions({}, player));
}

TEST(PhaseConditionEvaluatorTests, QuestRewardedRequired) {
  PhaseCondition cond;
  cond.type = PhaseConditionType::QuestRewarded;
  cond.value1 = 28598u;

  FakeQuestProgress player;
  EXPECT_FALSE(EvaluatePhaseConditions({cond}, player));

  player.SetRewarded(28598u);
  EXPECT_TRUE(EvaluatePhaseConditions({cond}, player));
}

TEST(PhaseConditionEvaluatorTests, QuestCompleteNotRewarded) {
  PhaseCondition cond;
  cond.type = PhaseConditionType::QuestComplete;
  cond.value1 = 28598u;

  FakeQuestProgress incomplete;
  incomplete.SetStatus(28598u, QuestStatus::Incomplete);
  EXPECT_FALSE(EvaluatePhaseConditions({cond}, incomplete));

  FakeQuestProgress complete;
  complete.SetStatus(28598u, QuestStatus::Complete);
  EXPECT_TRUE(EvaluatePhaseConditions({cond}, complete));

  FakeQuestProgress rewarded;
  rewarded.SetRewarded(28598u);
  EXPECT_FALSE(EvaluatePhaseConditions({cond}, rewarded));
}

TEST(PhaseConditionEvaluatorTests, HighbankPhase361BeforeQuestElseGroup) {
  PhaseCondition notRewarded;
  notRewarded.elseGroup = 1;
  notRewarded.type = PhaseConditionType::QuestRewarded;
  notRewarded.value1 = 28598u;
  notRewarded.negative = true;

  PhaseCondition notComplete;
  notComplete.elseGroup = 2;
  notComplete.type = PhaseConditionType::QuestComplete;
  notComplete.value1 = 28598u;
  notComplete.negative = true;

  FakeQuestProgress fresh;
  EXPECT_TRUE(EvaluatePhaseConditions({notRewarded, notComplete}, fresh));
}

TEST(PhaseConditionEvaluatorTests, HighbankPhase169AfterQuestRewarded) {
  PhaseCondition rewarded;
  rewarded.elseGroup = 1;
  rewarded.type = PhaseConditionType::QuestRewarded;
  rewarded.value1 = 28598u;

  PhaseCondition completeNotRewarded;
  completeNotRewarded.elseGroup = 2;
  completeNotRewarded.type = PhaseConditionType::QuestComplete;
  completeNotRewarded.value1 = 28598u;

  FakeQuestProgress player;
  player.SetRewarded(28598u);
  EXPECT_TRUE(EvaluatePhaseConditions({rewarded, completeNotRewarded}, player));
}

TEST(PhaseConditionEvaluatorTests, AuraConditionChecksSpellId) {
  PhaseCondition cond;
  cond.type = PhaseConditionType::Aura;
  cond.value1 = 88111u;

  FakeQuestProgress player;
  EXPECT_FALSE(EvaluatePhaseConditions({cond}, player));

  player.SetAura(88111u);
  EXPECT_TRUE(EvaluatePhaseConditions({cond}, player));
}

} // namespace
} // namespace Firelands
