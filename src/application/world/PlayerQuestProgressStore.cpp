#include <application/world/PlayerQuestProgressStore.h>

namespace Firelands {

void PlayerQuestProgressStore::LoadSnapshot(PlayerQuestProgressSnapshot snapshot) {
  m_activeQuests = std::move(snapshot.activeQuests);
  m_rewardedQuests = std::move(snapshot.rewardedQuests);
}

void PlayerQuestProgressStore::SetAuraChecker(std::function<bool(uint32 spellId)> checker) {
  m_hasAura = std::move(checker);
}

QuestStatus PlayerQuestProgressStore::GetQuestStatus(uint32 questId) const {
  if (auto const it = m_activeQuests.find(questId); it != m_activeQuests.end())
    return it->second;
  return QuestStatus::None;
}

bool PlayerQuestProgressStore::IsQuestRewarded(uint32 questId) const {
  return m_rewardedQuests.count(questId) != 0;
}

bool PlayerQuestProgressStore::HasAuraSpell(uint32 spellId) const {
  return m_hasAura && m_hasAura(spellId);
}

void PlayerQuestProgressStore::SetQuestStatus(uint32 questId, QuestStatus status) {
  if (status == QuestStatus::None) {
    m_activeQuests.erase(questId);
    return;
  }
  m_activeQuests[questId] = status;
}

void PlayerQuestProgressStore::SetQuestRewarded(uint32 questId) {
  m_rewardedQuests.insert(questId);
  m_activeQuests.erase(questId);
}

} // namespace Firelands
