#include "MySqlQuestGossipRepository.h"
#include <optional>
#include <string>
#include <shared/Logger.h>

namespace Firelands {

namespace {

char const kQuestSelectBase[] =
    "q.`ID`, q.`LogTitle`, q.`QuestDescription`, q.`LogDescription`, "
    "q.`QuestLevel`, q.`Flags`, q.`AllowableClasses`, q.`AllowableRaces`";

char const kQuestSelectSort[] = ", q.`QuestSortID`";

char const kQuestSelectGameplay[] =
    ", q.`SoundAccept`, q.`SoundTurnIn`, "
    "q.`RequiredNpcOrGo1`, q.`RequiredNpcOrGo2`, q.`RequiredNpcOrGo3`, "
    "q.`RequiredNpcOrGo4`, "
    "q.`RequiredNpcOrGoCount1`, q.`RequiredNpcOrGoCount2`, "
    "q.`RequiredNpcOrGoCount3`, q.`RequiredNpcOrGoCount4`, "
    "q.`RequiredItemId1`, q.`RequiredItemId2`, q.`RequiredItemId3`, "
    "q.`RequiredItemId4`, q.`RequiredItemId5`, q.`RequiredItemId6`, "
    "q.`RequiredItemCount1`, q.`RequiredItemCount2`, q.`RequiredItemCount3`, "
    "q.`RequiredItemCount4`, q.`RequiredItemCount5`, q.`RequiredItemCount6`";

void LoadGameplayColumns(sql::ResultSet &rs, QuestGossipSummary &summary) {
  summary.soundAccept = static_cast<uint16_t>(rs.getUInt("SoundAccept"));
  summary.soundTurnIn = static_cast<uint16_t>(rs.getUInt("SoundTurnIn"));
  summary.requiredNpcOrGo[0] = rs.getInt("RequiredNpcOrGo1");
  summary.requiredNpcOrGo[1] = rs.getInt("RequiredNpcOrGo2");
  summary.requiredNpcOrGo[2] = rs.getInt("RequiredNpcOrGo3");
  summary.requiredNpcOrGo[3] = rs.getInt("RequiredNpcOrGo4");
  summary.requiredNpcOrGoCount[0] =
      static_cast<uint16_t>(rs.getUInt("RequiredNpcOrGoCount1"));
  summary.requiredNpcOrGoCount[1] =
      static_cast<uint16_t>(rs.getUInt("RequiredNpcOrGoCount2"));
  summary.requiredNpcOrGoCount[2] =
      static_cast<uint16_t>(rs.getUInt("RequiredNpcOrGoCount3"));
  summary.requiredNpcOrGoCount[3] =
      static_cast<uint16_t>(rs.getUInt("RequiredNpcOrGoCount4"));
  summary.requiredItemId[0] = rs.getUInt("RequiredItemId1");
  summary.requiredItemId[1] = rs.getUInt("RequiredItemId2");
  summary.requiredItemId[2] = rs.getUInt("RequiredItemId3");
  summary.requiredItemId[3] = rs.getUInt("RequiredItemId4");
  summary.requiredItemId[4] = rs.getUInt("RequiredItemId5");
  summary.requiredItemId[5] = rs.getUInt("RequiredItemId6");
  summary.requiredItemCount[0] =
      static_cast<uint16_t>(rs.getUInt("RequiredItemCount1"));
  summary.requiredItemCount[1] =
      static_cast<uint16_t>(rs.getUInt("RequiredItemCount2"));
  summary.requiredItemCount[2] =
      static_cast<uint16_t>(rs.getUInt("RequiredItemCount3"));
  summary.requiredItemCount[3] =
      static_cast<uint16_t>(rs.getUInt("RequiredItemCount4"));
  summary.requiredItemCount[4] =
      static_cast<uint16_t>(rs.getUInt("RequiredItemCount5"));
  summary.requiredItemCount[5] =
      static_cast<uint16_t>(rs.getUInt("RequiredItemCount6"));
}

QuestGossipSummary LoadQuestSummaryFromRow(sql::ResultSet &rs, bool hasTextColumns,
                                           bool hasGameplayColumns,
                                           bool hasQuestSortId) {
  QuestGossipSummary summary;
  summary.questId = rs.getUInt("ID");
  summary.title = rs.getString("LogTitle");
  if (hasTextColumns) {
    summary.questDescription = rs.getString("QuestDescription");
    summary.logDescription = rs.getString("LogDescription");
  }
  summary.questLevel = rs.getInt("QuestLevel");
  if (hasQuestSortId)
    summary.questSortId = rs.getInt("QuestSortID");
  summary.flags = rs.getUInt("Flags");
  summary.allowableClasses = rs.getUInt("AllowableClasses");
  summary.allowableRaces = rs.getUInt("AllowableRaces");
  summary.blueQuestionMark = QuestGossipUsesBlueQuestionMark(summary.flags);
  if (hasGameplayColumns)
    LoadGameplayColumns(rs, summary);
  return summary;
}

std::vector<QuestGossipSummary> LoadCreatureLinkedQuests(
    sql::Connection &connection, char const *linkTable, uint32_t creatureEntry) {
  std::vector<QuestGossipSummary> result;
  if (creatureEntry == 0)
    return result;

  auto const runQuery = [&](char const *selectList, bool hasText, bool hasGameplay,
                            bool hasQuestSortId) {
    std::string const sql = std::string("SELECT ") + selectList + " FROM `" +
                            linkTable + "` cqs "
                            "INNER JOIN `quest_template` q ON q.`ID` = cqs.`quest` "
                            "WHERE cqs.`id` = ? "
                            "ORDER BY q.`ID`";
    std::unique_ptr<sql::PreparedStatement> stmt(connection.prepareStatement(sql));
    stmt->setUInt(1, creatureEntry);
    std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
    while (rs->next())
      result.push_back(
          LoadQuestSummaryFromRow(*rs, hasText, hasGameplay, hasQuestSortId));
  };

  try {
    std::string const full = std::string(kQuestSelectBase) + kQuestSelectSort +
                             kQuestSelectGameplay;
    runQuery(full.c_str(), true, true, true);
    return result;
  } catch (sql::SQLException const &e) {
    LOG_WARN("quest_template columns missing for {} entry={} ({}); run migrations "
             "65/66",
             linkTable, creatureEntry, e.what());
  }

  result.clear();
  try {
    std::string const withSort =
        std::string(kQuestSelectBase) + kQuestSelectSort;
    runQuery(withSort.c_str(), true, false, true);
    return result;
  } catch (sql::SQLException const &) {
  }

  result.clear();
  try {
    runQuery(kQuestSelectBase, true, false, false);
    return result;
  } catch (sql::SQLException const &) {
  }

  try {
    result.clear();
    runQuery("q.`ID`, q.`LogTitle`, q.`QuestLevel`, q.`Flags`, "
             "q.`AllowableClasses`, q.`AllowableRaces`",
             false, false, false);
  } catch (sql::SQLException const &e2) {
    LOG_ERROR("LoadCreatureLinkedQuests {} entry={}: {}", linkTable, creatureEntry,
              e2.what());
  }
  return result;
}

} // namespace

MySqlQuestGossipRepository::MySqlQuestGossipRepository(
    std::shared_ptr<sql::Connection> connection)
    : _connection(std::move(connection)) {}

std::vector<QuestGossipSummary>
MySqlQuestGossipRepository::GetStarterQuestsForCreature(
    uint32_t creatureEntry) const {
  return LoadCreatureLinkedQuests(*_connection, "creature_queststarter",
                                  creatureEntry);
}

std::vector<QuestGossipSummary>
MySqlQuestGossipRepository::GetEnderQuestsForCreature(
    uint32_t creatureEntry) const {
  try {
    return LoadCreatureLinkedQuests(*_connection, "creature_questender",
                                    creatureEntry);
  } catch (sql::SQLException const &e) {
    LOG_WARN("creature_questender unavailable ({}); run migration "
             "66_world_creature_questender_and_sort.sql",
             e.what());
    return {};
  }
}

std::optional<QuestGossipSummary>
MySqlQuestGossipRepository::TryGetQuestTemplate(uint32_t questId) const {
  if (questId == 0)
    return std::nullopt;

  auto const tryQuery = [&](char const *selectList, bool hasText, bool hasGameplay,
                            bool hasQuestSortId) -> std::optional<QuestGossipSummary> {
    std::string const sql =
        std::string("SELECT ") + selectList + " FROM `quest_template` WHERE `ID` = ? LIMIT 1";
    std::unique_ptr<sql::PreparedStatement> stmt(_connection->prepareStatement(sql));
    stmt->setUInt(1, questId);
    std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
    if (!rs->next())
      return std::nullopt;
    return LoadQuestSummaryFromRow(*rs, hasText, hasGameplay, hasQuestSortId);
  };

  try {
    std::string const full = std::string(kQuestSelectBase) + kQuestSelectSort +
                             kQuestSelectGameplay;
    if (auto summary = tryQuery(full.c_str(), true, true, true))
      return summary;
  } catch (sql::SQLException const &) {
  }

  try {
    std::string const withSort = std::string(kQuestSelectBase) + kQuestSelectSort;
    if (auto summary = tryQuery(withSort.c_str(), true, false, true))
      return summary;
  } catch (sql::SQLException const &) {
  }

  try {
    if (auto summary = tryQuery(kQuestSelectBase, true, false, false))
      return summary;
  } catch (sql::SQLException const &) {
  }

  try {
    return tryQuery("ID, LogTitle, QuestLevel, Flags, AllowableClasses, AllowableRaces",
                    false, false, false);
  } catch (sql::SQLException const &e) {
    LOG_ERROR("MySqlQuestGossipRepository::TryGetQuestTemplate id={}: {}", questId,
              e.what());
  }
  return std::nullopt;
}

} // namespace Firelands
