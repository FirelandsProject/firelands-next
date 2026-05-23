#include <infrastructure/persistence/MySqlPlayerCreateInfoRepository.h>
#include <shared/game/QuestMask.h>
#include <shared/Logger.h>
#include <cstdint>
#include <unordered_set>

namespace Firelands {

namespace {

bool IsMissingTableError(sql::SQLException &e) {
  // MariaDB connector-cpp accessors are not const-qualified.
  return e.getErrorCode() == 1146 || e.getSQLState() == "42S02";
}

/// Matches `sql/10_playercreateinfo_spawn.sql` so character creation works when
/// world migrations were skipped or failed.
bool EnsurePlayercreateinfoTable(std::shared_ptr<sql::Connection> conn) {
  try {
    std::unique_ptr<sql::Statement> st(conn->createStatement());
    st->execute("CREATE DATABASE IF NOT EXISTS `firelands_world`");
    st->execute(
        "CREATE TABLE IF NOT EXISTS `firelands_world`.`playercreateinfo` ("
        "`race` tinyint unsigned NOT NULL DEFAULT 0,"
        "`class` tinyint unsigned NOT NULL DEFAULT 0,"
        "`map` smallint unsigned NOT NULL DEFAULT 0,"
        "`zone` int unsigned NOT NULL DEFAULT 0,"
        "`position_x` float NOT NULL DEFAULT 0,"
        "`position_y` float NOT NULL DEFAULT 0,"
        "`position_z` float NOT NULL DEFAULT 0,"
        "`orientation` float NOT NULL DEFAULT 0,"
        "PRIMARY KEY (`race`,`class`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
    LOG_WARN(
        "Created missing `firelands_world.playercreateinfo` (empty). Apply sql "
        "migrations or insert spawn rows; using fallback start positions until "
        "then.");
    return true;
  } catch (sql::SQLException const &e) {
    LOG_ERROR("EnsurePlayercreateinfoTable failed: {}", e.what());
    return false;
  }
}

} // namespace

std::optional<PlayerCreateInfo>
MySqlPlayerCreateInfoRepository::GetStartPosition(uint8 race, uint8 klass) {
  for (int attempt = 0; attempt < 2; ++attempt) {
    try {
      std::unique_ptr<sql::PreparedStatement> stmt(
          m_connection->prepareStatement(
              "SELECT map, zone, position_x, position_y, position_z, orientation "
              "FROM firelands_world.playercreateinfo "
              "WHERE race = ? AND class = ? "
              "LIMIT 1"));
      stmt->setUInt(1, race);
      stmt->setUInt(2, klass);

      std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
      if (!rs->next())
        return std::nullopt;

      PlayerCreateInfo info;
      info.mapId = static_cast<uint16>(rs->getUInt("map"));
      info.zoneId = rs->getUInt("zone");
      info.x = rs->getDouble("position_x");
      info.y = rs->getDouble("position_y");
      info.z = rs->getDouble("position_z");
      info.orientation = rs->getDouble("orientation");
      return info;
    } catch (sql::SQLException &e) {
      if (attempt == 0 && IsMissingTableError(e) &&
          EnsurePlayercreateinfoTable(m_connection)) {
        continue;
      }
      LOG_ERROR("GetStartPosition query failed: {}", e.what());
      return std::nullopt;
    }
  }
  return std::nullopt;
}

std::vector<PlayerCreateVisualItem> MySqlPlayerCreateInfoRepository::GetVisualItems(
    uint8 race, uint8 klass, uint8 gender, uint8 outfitId) {
  std::vector<PlayerCreateVisualItem> rows;
  try {
    std::unique_ptr<sql::PreparedStatement> stmt(m_connection->prepareStatement(
        "SELECT slot, invType, displayId, displayEnchantId "
        "FROM firelands_world.playercreateinfo_visual_items "
        "WHERE race IN (0, ?) "
        "  AND class IN (0, ?) "
        "  AND gender IN (2, ?) "
        "  AND outfitId IN (0, ?) "
        "ORDER BY "
        "  (race = ?) DESC, "
        "  (class = ?) DESC, "
        "  (gender = ?) DESC, "
        "  (outfitId = ?) DESC"));

    stmt->setUInt(1, race);
    stmt->setUInt(2, klass);
    stmt->setUInt(3, gender);
    stmt->setUInt(4, outfitId);
    stmt->setUInt(5, race);
    stmt->setUInt(6, klass);
    stmt->setUInt(7, gender);
    stmt->setUInt(8, outfitId);

    std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
    std::unordered_set<uint8> seenSlots;
    while (rs->next()) {
      uint8 slot = static_cast<uint8>(rs->getUInt("slot"));
      if (slot >= 23)
        continue;
      if (seenSlots.find(slot) != seenSlots.end())
        continue;
      seenSlots.insert(slot);

      PlayerCreateVisualItem item;
      item.slot = slot;
      item.invType = static_cast<uint8>(rs->getUInt("invType"));
      item.displayId = rs->getUInt("displayId");
      item.displayEnchantId = rs->getUInt("displayEnchantId");
      rows.push_back(item);
    }
  } catch (sql::SQLException const &e) {
    LOG_ERROR("GetVisualItems query failed: {}", e.what());
  }

  return rows;
}

std::vector<uint32_t> MySqlPlayerCreateInfoRepository::GetStarterSpells(
    uint8_t race, uint8_t klass) {
  std::vector<uint32_t> out;
  std::unordered_set<uint32_t> seen;
  uint32_t const raceMask = PlayerRaceMask(race);
  uint32_t const classMask = PlayerClassMask(klass);
  try {
    std::unique_ptr<sql::PreparedStatement> stmt(m_connection->prepareStatement(
        "SELECT spellId FROM firelands_world.playercreateinfo_spell "
        "WHERE (raceMask = 0 OR (raceMask & ?) != 0) "
        "  AND (classMask = 0 OR (classMask & ?) != 0)"));
    stmt->setUInt(1, raceMask);
    stmt->setUInt(2, classMask);
    std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
    while (rs->next()) {
      uint32_t const sid = rs->getUInt("spellId");
      if (sid != 0u && seen.insert(sid).second)
        out.push_back(sid);
    }
  } catch (sql::SQLException &e) {
    if (!IsMissingTableError(e))
      LOG_ERROR("GetStarterSpells query failed: {}", e.what());
  }
  return out;
}

std::vector<uint32_t> MySqlPlayerCreateInfoRepository::GetRacialStarterSpells(
    uint8_t race, uint8_t klass) {
  std::vector<uint32_t> out;
  std::unordered_set<uint32_t> seen;
  uint32_t const raceMask = PlayerRaceMask(race);
  uint32_t const classMask = PlayerClassMask(klass);
  try {
    std::unique_ptr<sql::PreparedStatement> stmt(m_connection->prepareStatement(
        "SELECT spellId FROM firelands_world.playercreateinfo_spell "
        "WHERE raceMask != 0 AND raceMask != 4294967295 "
        "  AND (raceMask & ?) != 0 "
        "  AND (classMask = 0 OR classMask = 4294967295 OR (classMask & ?) != 0)"));
    stmt->setUInt(1, raceMask);
    stmt->setUInt(2, classMask);
    std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
    while (rs->next()) {
      uint32_t const sid = rs->getUInt("spellId");
      if (sid != 0u && seen.insert(sid).second)
        out.push_back(sid);
    }
  } catch (sql::SQLException &e) {
    if (!IsMissingTableError(e))
      LOG_ERROR("GetRacialStarterSpells query failed: {}", e.what());
  }
  return out;
}

std::vector<StarterSkillGrant> MySqlPlayerCreateInfoRepository::GetStarterSkills(
    uint8_t race, uint8_t klass) {
  std::vector<StarterSkillGrant> out;
  std::unordered_set<uint32_t> seen;
  uint32_t const raceMask = PlayerRaceMask(race);
  uint32_t const classMask = PlayerClassMask(klass);
  try {
    std::unique_ptr<sql::PreparedStatement> stmt(m_connection->prepareStatement(
        "SELECT skillId, `rank` FROM firelands_world.playercreateinfo_skill "
        "WHERE (raceMask = 0 OR (raceMask & ?) != 0) "
        "  AND (classMask = 0 OR (classMask & ?) != 0)"));
    stmt->setUInt(1, raceMask);
    stmt->setUInt(2, classMask);
    std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
    while (rs->next()) {
      uint32_t const skillId = rs->getUInt("skillId");
      if (skillId == 0u || skillId > 0xFFFFu || !seen.insert(skillId).second)
        continue;
      StarterSkillGrant g;
      g.skillId = skillId;
      uint16_t const rank = static_cast<uint16_t>(rs->getUInt("rank"));
      g.rank = rank;
      g.maxRank = rank;
      out.push_back(g);
    }
  } catch (sql::SQLException &e) {
    if (!IsMissingTableError(e))
      LOG_ERROR("GetStarterSkills query failed: {}", e.what());
  }
  return out;
}

std::optional<PlayerClassLevelStats>
MySqlPlayerCreateInfoRepository::GetClassLevelStats(uint8 klass, uint8 level) {
  auto queryRow = [&](uint8 k, uint8 lv) -> std::optional<PlayerClassLevelStats> {
    try {
      std::unique_ptr<sql::PreparedStatement> stmt(m_connection->prepareStatement(
          "SELECT `str`, `agi`, `sta`, `inte`, `spi` "
          "FROM firelands_world.player_classlevelstats "
          "WHERE `class` = ? AND `level` = ? LIMIT 1"));
      stmt->setUInt(1, k);
      stmt->setUInt(2, lv);
      std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
      if (!rs->next())
        return std::nullopt;
      PlayerClassLevelStats out;
      // Column index (1-based): avoids connector/metadata quirks with the name `inte`.
      out.str = static_cast<uint16_t>(rs->getUInt(1));
      out.agi = static_cast<uint16_t>(rs->getUInt(2));
      out.sta = static_cast<uint16_t>(rs->getUInt(3));
      out.inte = static_cast<uint16_t>(rs->getUInt(4));
      out.spi = static_cast<uint16_t>(rs->getUInt(5));
      return out;
    } catch (sql::SQLException &e) {
      if (!IsMissingTableError(e))
        LOG_ERROR("GetClassLevelStats query failed: {}", e.what());
      return std::nullopt;
    }
  };

  if (auto row = queryRow(klass, level))
    return row;
  if (level != 1)
    return queryRow(klass, 1);
  return std::nullopt;
}

std::optional<PlayerRaceStats>
MySqlPlayerCreateInfoRepository::GetRaceStats(uint8 race) {
  try {
    std::unique_ptr<sql::PreparedStatement> stmt(m_connection->prepareStatement(
        "SELECT `str`, `agi`, `sta`, `inte`, `spi` "
        "FROM firelands_world.player_racestats WHERE `race` = ? LIMIT 1"));
    stmt->setUInt(1, race);
    std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
    if (!rs->next())
      return std::nullopt;
    PlayerRaceStats out;
    out.str = static_cast<int16_t>(rs->getInt(1));
    out.agi = static_cast<int16_t>(rs->getInt(2));
    out.sta = static_cast<int16_t>(rs->getInt(3));
    out.inte = static_cast<int16_t>(rs->getInt(4));
    out.spi = static_cast<int16_t>(rs->getInt(5));
    return out;
  } catch (sql::SQLException &e) {
    if (!IsMissingTableError(e))
      LOG_ERROR("GetRaceStats query failed: {}", e.what());
    return std::nullopt;
  }
}

std::vector<StarterItemGrant>
MySqlPlayerCreateInfoRepository::GetExtraCreateItems(uint8 race, uint8 klass) {
  std::vector<StarterItemGrant> rows;
  try {
    std::unique_ptr<sql::PreparedStatement> stmt(m_connection->prepareStatement(
        "SELECT itemid, amount FROM firelands_world.playercreateinfo_item "
        "WHERE race IN (0, ?) AND class IN (0, ?) "
        "ORDER BY (race = ?) DESC, (class = ?) DESC"));
    stmt->setUInt(1, race);
    stmt->setUInt(2, klass);
    stmt->setUInt(3, race);
    stmt->setUInt(4, klass);

    std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
    std::unordered_set<uint32_t> seen;
    while (rs->next()) {
      uint32_t itemId = rs->getUInt("itemid");
      int32_t amt = rs->getInt("amount");
      if (itemId == 0 || amt <= 0 || seen.count(itemId))
        continue;
      seen.insert(itemId);
      StarterItemGrant g;
      g.itemId = itemId;
      g.count = static_cast<uint32_t>(amt);
      rows.push_back(g);
    }
  } catch (sql::SQLException const &e) {
    LOG_ERROR("GetExtraCreateItems query failed: {}", e.what());
  }
  return rows;
}

void MySqlPlayerCreateInfoRepository::ensureXpForLevelLoaded() const {
  if (m_xpForLevelLoadAttempted)
    return;
  m_xpForLevelLoadAttempted = true;
  try {
    std::unique_ptr<sql::Statement> st(m_connection->createStatement());
    std::unique_ptr<sql::ResultSet> rs(st->executeQuery(
        "SELECT `Level`, `Experience` FROM firelands_world.player_xp_for_level "
        "ORDER BY `Level`"));
    m_xpExperienceByLevel.assign(86u, 0u);
    while (rs->next()) {
      uint32_t const lvl = rs->getUInt("Level");
      uint32_t const xp = rs->getUInt("Experience");
      if (lvl > 0 && lvl < m_xpExperienceByLevel.size() && xp > 0)
        m_xpExperienceByLevel[lvl] = xp;
    }
  } catch (sql::SQLException &e) {
    if (IsMissingTableError(e))
      LOG_WARN(
          "player_xp_for_level missing in firelands_world; XP bar uses fallback "
          "until firelands_world includes `player_xp_for_level` "
          "(deploy bundle: sql/bundled/firelands_world.sql).");
    else
      LOG_ERROR("ensureXpForLevelLoaded failed: {}", e.what());
    m_xpExperienceByLevel.clear();
  }
}

uint32_t
MySqlPlayerCreateInfoRepository::GetXpForNextLevel(uint8_t currentLevel) const {
  if (currentLevel == 0 || currentLevel >= 85)
    return 0;
  ensureXpForLevelLoaded();
  if (currentLevel < m_xpExperienceByLevel.size() &&
      m_xpExperienceByLevel[currentLevel] != 0)
    return m_xpExperienceByLevel[currentLevel];
  return 0;
}

} // namespace Firelands
