#include "MySqlQuestGossipRepository.h"
#include <optional>
#include <string>
#include <shared/Logger.h>

namespace Firelands {

namespace {

char const kQuestSelectCore[] =
    "q.`ID`, q.`LogTitle`, q.`QuestDescription`, q.`LogDescription`, "
    "q.`QuestLevel`, q.`Flags`, q.`AllowableClasses`, q.`AllowableRaces`";

char const kQuestSelectPrev[] = ", q.`PrevQuestId`";

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
                                           bool hasQuestSortId, bool hasPrevQuestId) {
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
  if (hasPrevQuestId)
    summary.prevQuestId = rs.getInt("PrevQuestId");
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
                            bool hasQuestSortId, bool hasPrevQuestId) {
    std::string const sql = std::string("SELECT ") + selectList + " FROM `" +
                            linkTable + "` cqs "
                            "INNER JOIN `quest_template` q ON q.`ID` = cqs.`quest` "
                            "WHERE cqs.`id` = ? "
                            "ORDER BY q.`ID`";
    std::unique_ptr<sql::PreparedStatement> stmt(connection.prepareStatement(sql));
    stmt->setUInt(1, creatureEntry);
    std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
    while (rs->next())
      result.push_back(LoadQuestSummaryFromRow(*rs, hasText, hasGameplay, hasQuestSortId,
                                               hasPrevQuestId));
  };

  // Prefer PrevQuestId when present (migration 67+) so quest chains gate correctly.
  try {
    std::string const withPrev = std::string(kQuestSelectCore) + kQuestSelectPrev +
                                 kQuestSelectSort + kQuestSelectGameplay;
    runQuery(withPrev.c_str(), true, true, true, true);
    return result;
  } catch (sql::SQLException const &) {
  }

  result.clear();
  try {
    std::string const full =
        std::string(kQuestSelectCore) + kQuestSelectSort + kQuestSelectGameplay;
    runQuery(full.c_str(), true, true, true, false);
    return result;
  } catch (sql::SQLException const &e) {
    LOG_WARN("quest_template columns missing for {} entry={} ({}); run migrations 65/66",
             linkTable, creatureEntry, e.what());
  }

  result.clear();
  try {
    std::string const withSort = std::string(kQuestSelectCore) + kQuestSelectSort;
    runQuery(withSort.c_str(), true, false, true, false);
    return result;
  } catch (sql::SQLException const &) {
  }

  result.clear();
  try {
    runQuery(kQuestSelectCore, true, false, false, false);
    return result;
  } catch (sql::SQLException const &) {
  }

  try {
    result.clear();
    runQuery("q.`ID`, q.`LogTitle`, q.`QuestLevel`, q.`Flags`, "
             "q.`AllowableClasses`, q.`AllowableRaces`",
             false, false, false, false);
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

std::vector<QuestGossipSummary> MySqlQuestGossipRepository::LoadCachedCreatureQuests(
    uint32_t creatureEntry, char const *linkTable,
    std::unordered_map<uint32_t, std::vector<QuestGossipSummary>> &cache) const {
  {
    std::lock_guard lock(_cacheMutex);
    if (auto const it = cache.find(creatureEntry); it != cache.end())
      return it->second;
  }
  std::vector<QuestGossipSummary> loaded =
      LoadCreatureLinkedQuests(*_connection, linkTable, creatureEntry);
  {
    std::lock_guard lock(_cacheMutex);
    cache.emplace(creatureEntry, loaded);
  }
  return loaded;
}

std::vector<QuestGossipSummary>
MySqlQuestGossipRepository::GetStarterQuestsForCreature(
    uint32_t creatureEntry) const {
  return LoadCachedCreatureQuests(creatureEntry, "creature_queststarter",
                                  _starterCache);
}

std::vector<QuestGossipSummary>
MySqlQuestGossipRepository::GetEnderQuestsForCreature(
    uint32_t creatureEntry) const {
  try {
    return LoadCachedCreatureQuests(creatureEntry, "creature_questender",
                                    _enderCache);
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
                            bool hasQuestSortId,
                            bool hasPrevQuestId) -> std::optional<QuestGossipSummary> {
    std::string const sql =
        std::string("SELECT ") + selectList + " FROM `quest_template` WHERE `ID` = ? LIMIT 1";
    std::unique_ptr<sql::PreparedStatement> stmt(_connection->prepareStatement(sql));
    stmt->setUInt(1, questId);
    std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
    if (!rs->next())
      return std::nullopt;
    return LoadQuestSummaryFromRow(*rs, hasText, hasGameplay, hasQuestSortId,
                                 hasPrevQuestId);
  };

  try {
    std::string const withPrev = std::string(kQuestSelectCore) + kQuestSelectPrev +
                                 kQuestSelectSort + kQuestSelectGameplay;
    if (auto summary = tryQuery(withPrev.c_str(), true, true, true, true))
      return summary;
  } catch (sql::SQLException const &) {
  }

  try {
    std::string const full =
        std::string(kQuestSelectCore) + kQuestSelectSort + kQuestSelectGameplay;
    if (auto summary = tryQuery(full.c_str(), true, true, true, false))
      return summary;
  } catch (sql::SQLException const &) {
  }

  try {
    std::string const withSort = std::string(kQuestSelectCore) + kQuestSelectSort;
    if (auto summary = tryQuery(withSort.c_str(), true, false, true, false))
      return summary;
  } catch (sql::SQLException const &) {
  }

  try {
    if (auto summary = tryQuery(kQuestSelectCore, true, false, false, false))
      return summary;
  } catch (sql::SQLException const &) {
  }

  try {
    return tryQuery("ID, LogTitle, QuestLevel, Flags, AllowableClasses, AllowableRaces",
                    false, false, false, false);
  } catch (sql::SQLException const &e) {
    LOG_ERROR("MySqlQuestGossipRepository::TryGetQuestTemplate id={}: {}", questId,
              e.what());
  }
  return std::nullopt;
}

std::vector<uint32_t>
MySqlQuestGossipRepository::LoadInvolvedCreatureEntries(uint32_t questId) const {
  std::vector<uint32_t> entries;
  if (!_connection || questId == 0)
    return entries;

  try {
    std::unique_ptr<sql::PreparedStatement> stmt(_connection->prepareStatement(
        "SELECT `id` FROM `creature_queststarter` WHERE `quest` = ? "
        "UNION "
        "SELECT `id` FROM `creature_questender` WHERE `quest` = ? "
        "ORDER BY `id`"));
    stmt->setUInt(1, questId);
    stmt->setUInt(2, questId);
    std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
    while (rs->next())
      entries.push_back(rs->getUInt("id"));
  } catch (sql::SQLException const &e) {
    LOG_WARN("GetInvolvedCreatureEntriesForQuest quest={}: {}", questId, e.what());
  }
  return entries;
}

std::vector<uint32_t>
MySqlQuestGossipRepository::GetInvolvedCreatureEntriesForQuest(
    uint32_t questId) const {
  if (questId == 0)
    return {};
  {
    std::lock_guard lock(_cacheMutex);
    if (auto const it = _questCreatureCache.find(questId); it != _questCreatureCache.end())
      return it->second;
  }
  std::vector<uint32_t> const loaded = LoadInvolvedCreatureEntries(questId);
  {
    std::lock_guard lock(_cacheMutex);
    _questCreatureCache.emplace(questId, loaded);
  }
  return loaded;
}

} // namespace Firelands
