#include "MySqlPlayerQuestProgressRepository.h"

#include <shared/Logger.h>

namespace Firelands {

namespace {

QuestStatus MapQuestStatus(uint8 raw) {
  switch (raw) {
  case 1:
    return QuestStatus::Complete;
  case 3:
    return QuestStatus::Incomplete;
  case 5:
    return QuestStatus::Failed;
  default:
    return QuestStatus::None;
  }
}

} // namespace

MySqlPlayerQuestProgressRepository::MySqlPlayerQuestProgressRepository(
    std::shared_ptr<sql::Connection> connection)
    : m_connection(std::move(connection)) {}

PlayerQuestProgressSnapshot
MySqlPlayerQuestProgressRepository::LoadForCharacter(uint32 characterGuid) const {
  PlayerQuestProgressSnapshot snapshot;
  if (!m_connection || characterGuid == 0)
    return snapshot;

  try {
    std::unique_ptr<sql::PreparedStatement> active(
        m_connection->prepareStatement(
            "SELECT `quest`, `status` FROM `character_queststatus` WHERE `guid` = ?"));
    active->setUInt(1, characterGuid);
    std::unique_ptr<sql::ResultSet> activeRes(active->executeQuery());
    while (activeRes->next()) {
      uint32 const questId = activeRes->getUInt(1);
      QuestStatus const status = MapQuestStatus(activeRes->getUInt(2));
      if (questId != 0 && status != QuestStatus::None)
        snapshot.activeQuests.emplace(questId, status);
    }

    std::unique_ptr<sql::PreparedStatement> rewarded(
        m_connection->prepareStatement(
            "SELECT `quest` FROM `character_queststatus_rewarded` WHERE `guid` = ?"));
    rewarded->setUInt(1, characterGuid);
    std::unique_ptr<sql::ResultSet> rewardedRes(rewarded->executeQuery());
    while (rewardedRes->next()) {
      uint32 const questId = rewardedRes->getUInt(1);
      if (questId != 0)
        snapshot.rewardedQuests.insert(questId);
    }
  } catch (sql::SQLException const &e) {
    LOG_WARN("character quest progress not loaded for guid {} ({}); phase gates use "
             "empty quest state",
             characterGuid, e.what());
  }

  return snapshot;
}

} // namespace Firelands
