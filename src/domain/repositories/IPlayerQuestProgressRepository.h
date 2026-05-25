#pragma once

#include <domain/models/QuestProgress.h>
#include <shared/Common.h>

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace Firelands {

struct PlayerQuestProgressSnapshot {
  std::unordered_map<uint32, QuestStatus> activeQuests;
  std::unordered_set<uint32> rewardedQuests;
};

/// Loads persisted quest progress for phase condition checks.
class IPlayerQuestProgressRepository {
public:
  virtual ~IPlayerQuestProgressRepository() = default;

  virtual PlayerQuestProgressSnapshot LoadForCharacter(uint32 characterGuid) const = 0;

  /// Replaces persisted rows for `characterGuid` (logout and after quest changes).
  virtual bool SaveForCharacter(uint32 characterGuid,
                                PlayerQuestProgressSnapshot const &snapshot) const = 0;
};

} // namespace Firelands
