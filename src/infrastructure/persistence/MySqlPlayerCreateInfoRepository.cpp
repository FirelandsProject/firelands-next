#include <infrastructure/persistence/MySqlPlayerCreateInfoRepository.h>
#include <shared/Logger.h>
#include <unordered_set>

namespace Firelands {

namespace {

bool IsMissingTableError(sql::SQLException &e) {
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

} // namespace Firelands
