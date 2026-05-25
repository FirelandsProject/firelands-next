#pragma once

#include <domain/repositories/IQuestGossipRepository.h>
#include <conncpp.hpp>
#include <memory>

namespace Firelands {

class MySqlQuestGossipRepository : public IQuestGossipRepository {
public:
  explicit MySqlQuestGossipRepository(std::shared_ptr<sql::Connection> connection);

  std::vector<QuestGossipSummary>
  GetStarterQuestsForCreature(uint32_t creatureEntry) const override;

  std::vector<QuestGossipSummary>
  GetEnderQuestsForCreature(uint32_t creatureEntry) const override;

  std::optional<QuestGossipSummary> TryGetQuestTemplate(uint32_t questId) const override;

private:
  std::shared_ptr<sql::Connection> _connection;
};

} // namespace Firelands
