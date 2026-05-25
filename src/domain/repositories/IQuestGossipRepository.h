#pragma once

#include <domain/models/QuestGossip.h>
#include <cstdint>
#include <optional>
#include <vector>

namespace Firelands {

/// Quests a creature can offer in gossip (`creature_queststarter` + `quest_template`).
class IQuestGossipRepository {
public:
  virtual ~IQuestGossipRepository() = default;

  virtual std::vector<QuestGossipSummary>
  GetStarterQuestsForCreature(uint32_t creatureEntry) const = 0;

  /// Turn-in quests (`creature_questender` + `quest_template`).
  virtual std::vector<QuestGossipSummary>
  GetEnderQuestsForCreature(uint32_t creatureEntry) const = 0;

  /// `CMSG_QUERY_QUEST_INFO` — any quest in `quest_template` (not only creature starters).
  virtual std::optional<QuestGossipSummary>
  TryGetQuestTemplate(uint32_t questId) const = 0;
};

} // namespace Firelands
