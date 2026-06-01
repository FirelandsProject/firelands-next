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

  /// Single starter row for accept/query (avoids repeated scans when lists are cached).
  virtual std::optional<QuestGossipSummary>
  TryGetStarterQuestForCreature(uint32_t creatureEntry, uint32_t questId) const {
    for (QuestGossipSummary const &summary :
         GetStarterQuestsForCreature(creatureEntry)) {
      if (summary.questId == questId)
        return summary;
    }
    return std::nullopt;
  }

  /// Turn-in quests (`creature_questender` + `quest_template`).
  virtual std::vector<QuestGossipSummary>
  GetEnderQuestsForCreature(uint32_t creatureEntry) const = 0;

  /// `CMSG_QUERY_QUEST_INFO` — any quest in `quest_template` (not only creature starters).
  virtual std::optional<QuestGossipSummary>
  TryGetQuestTemplate(uint32_t questId) const = 0;

  /// `CMSG_QUEST_NPC_QUERY` — creature (and GO) entries tied to a quest (map markers).
  virtual std::vector<uint32_t>
  GetInvolvedCreatureEntriesForQuest(uint32_t questId) const = 0;
};

} // namespace Firelands
