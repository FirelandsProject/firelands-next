#include <application/world/PlayerQuestProgressStore.h>

namespace Firelands {

void PlayerQuestProgressStore::LoadSnapshot(PlayerQuestProgressSnapshot snapshot) {
  m_activeQuests = std::move(snapshot.activeQuests);
  m_rewardedQuests = std::move(snapshot.rewardedQuests);
  RebuildQuestLogFromActive();
}

PlayerQuestProgressSnapshot PlayerQuestProgressStore::ExportSnapshot() const {
  PlayerQuestProgressSnapshot snapshot;
  snapshot.activeQuests = m_activeQuests;
  snapshot.rewardedQuests = m_rewardedQuests;
  return snapshot;
}

void PlayerQuestProgressStore::RebuildQuestLogFromActive() {
  m_questLogSlots.fill(0);
  uint8_t slot = 0;
  for (auto const &entry : m_activeQuests) {
    if (slot >= kMaxQuestLogSlots)
      break;
    m_questLogSlots[slot++] = entry.first;
  }
}

bool PlayerQuestProgressStore::HasFreeQuestLogSlot() const {
  for (uint32_t slotQuestId : m_questLogSlots) {
    if (slotQuestId == 0)
      return true;
  }
  return false;
}

std::optional<uint8_t> PlayerQuestProgressStore::FindQuestLogSlot(uint32 questId) const {
  if (questId == 0)
    return std::nullopt;
  for (uint8_t slot = 0; slot < kMaxQuestLogSlots; ++slot) {
    if (m_questLogSlots[slot] == questId)
      return slot;
  }
  return std::nullopt;
}

uint32_t PlayerQuestProgressStore::GetQuestLogSlotQuestId(uint8_t slot) const {
  if (slot >= kMaxQuestLogSlots)
    return 0;
  return m_questLogSlots[slot];
}

std::optional<uint8_t> PlayerQuestProgressStore::TryAssignQuestLogSlot(uint32 questId) {
  if (questId == 0)
    return std::nullopt;
  if (auto const existing = FindQuestLogSlot(questId))
    return existing;
  for (uint8_t slot = 0; slot < kMaxQuestLogSlots; ++slot) {
    if (m_questLogSlots[slot] == 0) {
      m_questLogSlots[slot] = questId;
      return slot;
    }
  }
  return std::nullopt;
}

void PlayerQuestProgressStore::ClearQuestLogSlot(uint32 questId) {
  if (auto const slot = FindQuestLogSlot(questId))
    m_questLogSlots[*slot] = 0;
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
    ClearQuestLogSlot(questId);
    return;
  }
  m_activeQuests[questId] = status;
  (void)TryAssignQuestLogSlot(questId);
}

void PlayerQuestProgressStore::SetQuestRewarded(uint32 questId) {
  m_rewardedQuests.insert(questId);
  m_activeQuests.erase(questId);
  ClearQuestLogSlot(questId);
}

} // namespace Firelands
