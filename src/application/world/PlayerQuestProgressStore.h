#pragma once

#include <domain/models/QuestProgress.h>
#include <domain/ports/IPlayerQuestProgress.h>
#include <domain/repositories/IPlayerQuestProgressRepository.h>

#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace Firelands {

/// In-memory quest/aura state for phase condition evaluation on a live session.
class PlayerQuestProgressStore : public IPlayerQuestProgress {
public:
  void LoadSnapshot(PlayerQuestProgressSnapshot snapshot);
  void SetAuraChecker(std::function<bool(uint32 spellId)> checker);

  QuestStatus GetQuestStatus(uint32 questId) const override;
  bool IsQuestRewarded(uint32 questId) const override;
  bool HasAuraSpell(uint32 spellId) const override;

  void SetQuestStatus(uint32 questId, QuestStatus status);
  void SetQuestRewarded(uint32 questId);

private:
  std::unordered_map<uint32, QuestStatus> m_activeQuests;
  std::unordered_set<uint32> m_rewardedQuests;
  std::function<bool(uint32 spellId)> m_hasAura;
};

} // namespace Firelands
