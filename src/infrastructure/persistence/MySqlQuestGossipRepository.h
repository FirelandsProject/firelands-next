#pragma once

#include <domain/repositories/IQuestGossipRepository.h>
#include <conncpp.hpp>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Firelands {

class MySqlQuestGossipRepository : public IQuestGossipRepository {
public:
  explicit MySqlQuestGossipRepository(std::shared_ptr<sql::Connection> connection);

  std::vector<QuestGossipSummary>
  GetStarterQuestsForCreature(uint32_t creatureEntry) const override;

  std::vector<QuestGossipSummary>
  GetEnderQuestsForCreature(uint32_t creatureEntry) const override;

  std::optional<QuestGossipSummary> TryGetQuestTemplate(uint32_t questId) const override;

  std::vector<uint32_t>
  GetInvolvedCreatureEntriesForQuest(uint32_t questId) const override;

private:
  std::vector<QuestGossipSummary> LoadCachedCreatureQuests(
      uint32_t creatureEntry, char const *linkTable,
      std::unordered_map<uint32_t, std::vector<QuestGossipSummary>> &cache) const;

  std::vector<uint32_t> LoadInvolvedCreatureEntries(uint32_t questId) const;

  std::shared_ptr<sql::Connection> _connection;
  mutable std::mutex _cacheMutex;
  mutable std::unordered_map<uint32_t, std::vector<QuestGossipSummary>> _starterCache;
  mutable std::unordered_map<uint32_t, std::vector<QuestGossipSummary>> _enderCache;
  mutable std::unordered_map<uint32_t, std::vector<uint32_t>> _questCreatureCache;
};

} // namespace Firelands
