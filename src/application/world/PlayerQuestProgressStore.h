#pragma once

#include <domain/models/QuestProgress.h>
#include <domain/ports/IPlayerQuestProgress.h>
#include <domain/repositories/IPlayerQuestProgressRepository.h>
#include <shared/game/PlayerQuestLog.h>

#include <array>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace Firelands {

/// In-memory quest/aura state for phase condition evaluation on a live session.
class PlayerQuestProgressStore : public IPlayerQuestProgress {
public:
  void LoadSnapshot(PlayerQuestProgressSnapshot snapshot);
  PlayerQuestProgressSnapshot ExportSnapshot() const;
  void SetAuraChecker(std::function<bool(uint32 spellId)> checker);

  QuestStatus GetQuestStatus(uint32 questId) const override;
  bool IsQuestRewarded(uint32 questId) const override;
  bool HasAuraSpell(uint32 spellId) const override;

  void SetQuestStatus(uint32 questId, QuestStatus status);
  void SetQuestRewarded(uint32 questId);

  /// Assigns or returns existing quest log slot (0..24). Empty when log is full.
  std::optional<uint8_t> TryAssignQuestLogSlot(uint32 questId);
  std::optional<uint8_t> FindQuestLogSlot(uint32 questId) const;
  uint32_t GetQuestLogSlotQuestId(uint8_t slot) const;
  bool HasFreeQuestLogSlot() const;
  void ClearQuestLogSlot(uint32 questId);

private:
  void RebuildQuestLogFromActive();

  std::unordered_map<uint32, QuestStatus> m_activeQuests;
  std::unordered_set<uint32> m_rewardedQuests;
  std::array<uint32_t, kMaxQuestLogSlots> m_questLogSlots{};
  std::function<bool(uint32 spellId)> m_hasAura;
};

} // namespace Firelands
