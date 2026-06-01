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

uint8_t MapQuestStatusToDb(QuestStatus status) {
  switch (status) {
  case QuestStatus::Complete:
    return 1;
  case QuestStatus::Incomplete:
    return 3;
  case QuestStatus::Failed:
    return 5;
  default:
    return 0;
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

bool MySqlPlayerQuestProgressRepository::SaveForCharacter(
    uint32 characterGuid, PlayerQuestProgressSnapshot const &snapshot) const {
  if (!m_connection || characterGuid == 0)
    return false;

  try {
    bool const hadAutoCommit = m_connection->getAutoCommit();
    m_connection->setAutoCommit(false);

    std::unique_ptr<sql::PreparedStatement> clearActive(
        m_connection->prepareStatement(
            "DELETE FROM `character_queststatus` WHERE `guid` = ?"));
    clearActive->setUInt(1, characterGuid);
    clearActive->executeUpdate();

    std::unique_ptr<sql::PreparedStatement> insertActive(
        m_connection->prepareStatement(
            "INSERT INTO `character_queststatus` (`guid`, `quest`, `status`) "
            "VALUES (?, ?, ?)"));
    for (auto const &[questId, status] : snapshot.activeQuests) {
      uint8_t const wire = MapQuestStatusToDb(status);
      if (questId == 0 || wire == 0)
        continue;
      insertActive->setUInt(1, characterGuid);
      insertActive->setUInt(2, questId);
      insertActive->setUInt(3, wire);
      insertActive->executeUpdate();
    }

    std::unique_ptr<sql::PreparedStatement> clearRewarded(
        m_connection->prepareStatement(
            "DELETE FROM `character_queststatus_rewarded` WHERE `guid` = ?"));
    clearRewarded->setUInt(1, characterGuid);
    clearRewarded->executeUpdate();

    std::unique_ptr<sql::PreparedStatement> insertRewarded(
        m_connection->prepareStatement(
            "INSERT INTO `character_queststatus_rewarded` (`guid`, `quest`) "
            "VALUES (?, ?)"));
    for (uint32 const questId : snapshot.rewardedQuests) {
      if (questId == 0)
        continue;
      insertRewarded->setUInt(1, characterGuid);
      insertRewarded->setUInt(2, questId);
      insertRewarded->executeUpdate();
    }

    m_connection->commit();
    m_connection->setAutoCommit(hadAutoCommit);
    return true;
  } catch (sql::SQLException const &e) {
    try {
      m_connection->rollback();
      m_connection->setAutoCommit(true);
    } catch (sql::SQLException const &) {
    }
    LOG_ERROR("SaveForCharacter quest progress failed for guid {}: {}", characterGuid,
              e.what());
    return false;
  }
}

} // namespace Firelands
