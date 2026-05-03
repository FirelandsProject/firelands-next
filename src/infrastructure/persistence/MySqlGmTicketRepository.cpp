#include "MySqlGmTicketRepository.h"
#include <shared/Logger.h>
#include <cstring>
#include <string>

namespace Firelands {

namespace {

uint8_t StatusToStored(GmTicketStatus s) {
  return static_cast<uint8_t>(s);
}

GmTicketStatus StoredToStatus(uint8_t v) {
  if (v > static_cast<uint8_t>(GmTicketStatus::ClosedStaff))
    return GmTicketStatus::Open;
  return static_cast<GmTicketStatus>(v);
}

bool IsMissingGmTicketTableError(sql::SQLException &e) {
  return e.getErrorCode() == 1146 || e.getSQLState() == "42S02" ||
         std::string{e.what()}.find("doesn't exist") != std::string::npos;
}

/// Keeps dev DBs working without a manual `sql/18_gm_ticket.sql` step; must stay
/// aligned with that file and `characters_schema.sql`.
bool EnsureGmTicketTable(std::shared_ptr<sql::Connection> conn) {
  try {
    std::unique_ptr<sql::Statement> st(conn->createStatement());
    st->execute(
        R"(CREATE TABLE IF NOT EXISTS `firelands_characters`.`gm_ticket` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `account_id` int unsigned NOT NULL COMMENT 'Player account that opened the ticket',
  `character_guid` int unsigned NOT NULL,
  `status` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '0=open 1=assigned 2=gm_answered 3=closed_resolved 4=closed_abandoned 5=closed_staff',
  `category` tinyint unsigned NOT NULL DEFAULT '0',
  `need_more_help` tinyint unsigned NOT NULL DEFAULT '0',
  `message` text NOT NULL,
  `gm_response` text NULL COMMENT 'Last GM reply shown to player when status is gm_answered',
  `map_id` smallint unsigned NOT NULL DEFAULT '0',
  `pos_x` float NOT NULL DEFAULT '0',
  `pos_y` float NOT NULL DEFAULT '0',
  `pos_z` float NOT NULL DEFAULT '0',
  `assigned_account_id` int unsigned NULL COMMENT 'Staff account.id currently handling the ticket',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `assigned_at` timestamp NULL DEFAULT NULL,
  `closed_at` timestamp NULL DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_character_open` (`character_guid`, `status`),
  KEY `idx_queue` (`status`, `created_at`),
  KEY `idx_assigned` (`assigned_account_id`, `status`),
  CONSTRAINT `fk_gm_ticket_character` FOREIGN KEY (`character_guid`) REFERENCES `firelands_characters`.`characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci)");
    return true;
  } catch (sql::SQLException &e) {
    LOG_ERROR("EnsureGmTicketTable failed: {}", e.what());
    return false;
  }
}

} // namespace

bool MySqlGmTicketRepository::EnsureNeedMoreHelpColumn(
    std::shared_ptr<sql::Connection> conn) {
  try {
    std::unique_ptr<sql::Statement> st(conn->createStatement());
    st->execute(
        "ALTER TABLE `firelands_characters`.`gm_ticket` "
        "ADD COLUMN `need_more_help` tinyint unsigned NOT NULL DEFAULT 0 "
        "AFTER `category`");
    LOG_DEBUG("Added column `firelands_characters.gm_ticket.need_more_help`.");
    return true;
  } catch (sql::SQLException &e) {
    if (e.getErrorCode() == 1060)
      return true;
    std::string const msg{e.what()};
    if (msg.find("Duplicate column") != std::string::npos)
      return true;
    if (IsMissingGmTicketTableError(e)) {
      LOG_ERROR(
          "gm_ticket missing after bootstrap; apply sql/18_gm_ticket.sql to "
          "firelands_characters (e.g. `characters` table or FK issue).");
      return false;
    }
    LOG_WARN("EnsureNeedMoreHelpColumn failed: {}", e.what());
    return false;
  }
}

MySqlGmTicketRepository::MySqlGmTicketRepository(
    std::shared_ptr<sql::Connection> connection)
    : _connection(std::move(connection)) {
  EnsureGmTicketTable(_connection);
  EnsureNeedMoreHelpColumn(_connection);
}

GmTicket MySqlGmTicketRepository::RowToTicket(sql::ResultSet &rs) {
  GmTicket t;
  t.id = rs.getUInt64("id");
  t.accountId = rs.getUInt("account_id");
  t.characterGuid = rs.getUInt("character_guid");
  t.status = StoredToStatus(static_cast<uint8_t>(rs.getUInt("status")));
  t.category = static_cast<uint8_t>(rs.getUInt("category"));
  t.needMoreHelp = rs.getUInt("need_more_help") != 0;
  t.message = rs.getString("message");
  if (!rs.isNull("gm_response"))
    t.gmResponse = rs.getString("gm_response");
  t.mapId = static_cast<uint16_t>(rs.getUInt("map_id"));
  t.posX = static_cast<float>(rs.getDouble("pos_x"));
  t.posY = static_cast<float>(rs.getDouble("pos_y"));
  t.posZ = static_cast<float>(rs.getDouble("pos_z"));
  if (!rs.isNull("assigned_account_id"))
    t.assignedAccountId = rs.getUInt("assigned_account_id");
  t.createdAtUnixMs =
      static_cast<uint64_t>(rs.getUInt64("created_unix")) * 1000ull;
  t.updatedAtUnixMs =
      static_cast<uint64_t>(rs.getUInt64("updated_unix")) * 1000ull;
  return t;
}

std::optional<GmTicket> MySqlGmTicketRepository::FindOpenByCharacterGuid(
    uint32_t characterGuid) {
  try {
    std::shared_ptr<sql::PreparedStatement> st(_connection->prepareStatement(
        "SELECT id, account_id, character_guid, status, category, need_more_help, "
        "message, gm_response, map_id, pos_x, pos_y, pos_z, assigned_account_id, "
        "UNIX_TIMESTAMP(created_at) AS created_unix, "
        "UNIX_TIMESTAMP(updated_at) AS updated_unix "
        "FROM firelands_characters.gm_ticket "
        "WHERE character_guid = ? AND status IN (0,1,2) "
        "ORDER BY id DESC LIMIT 1"));
    st->setUInt(1, characterGuid);
    std::unique_ptr<sql::ResultSet> rs(st->executeQuery());
    if (rs->next())
      return RowToTicket(*rs);
  } catch (sql::SQLException &e) {
    LOG_ERROR("FindOpenByCharacterGuid: {}", e.what());
  }
  return std::nullopt;
}

std::optional<GmTicket> MySqlGmTicketRepository::FindById(uint64_t ticketId) {
  try {
    std::shared_ptr<sql::PreparedStatement> st(_connection->prepareStatement(
        "SELECT id, account_id, character_guid, status, category, need_more_help, "
        "message, gm_response, map_id, pos_x, pos_y, pos_z, assigned_account_id, "
        "UNIX_TIMESTAMP(created_at) AS created_unix, "
        "UNIX_TIMESTAMP(updated_at) AS updated_unix "
        "FROM firelands_characters.gm_ticket WHERE id = ?"));
    st->setUInt64(1, ticketId);
    std::unique_ptr<sql::ResultSet> rs(st->executeQuery());
    if (rs->next())
      return RowToTicket(*rs);
  } catch (sql::SQLException &e) {
    LOG_ERROR("FindById: {}", e.what());
  }
  return std::nullopt;
}

std::vector<GmTicket> MySqlGmTicketRepository::ListUnassignedOpen(uint32_t limit) {
  std::vector<GmTicket> out;
  try {
    std::shared_ptr<sql::PreparedStatement> st(_connection->prepareStatement(
        "SELECT id, account_id, character_guid, status, category, need_more_help, "
        "message, gm_response, map_id, pos_x, pos_y, pos_z, assigned_account_id, "
        "UNIX_TIMESTAMP(created_at) AS created_unix, "
        "UNIX_TIMESTAMP(updated_at) AS updated_unix "
        "FROM firelands_characters.gm_ticket "
        "WHERE status = 0 AND assigned_account_id IS NULL "
        "ORDER BY created_at ASC LIMIT ?"));
    st->setUInt(1, limit);
    std::unique_ptr<sql::ResultSet> rs(st->executeQuery());
    while (rs->next())
      out.push_back(RowToTicket(*rs));
  } catch (sql::SQLException &e) {
    LOG_ERROR("ListUnassignedOpen: {}", e.what());
  }
  return out;
}

std::vector<GmTicket> MySqlGmTicketRepository::ListAssignedToAccount(
    uint32_t staffAccountId, uint32_t limit) {
  std::vector<GmTicket> out;
  try {
    std::shared_ptr<sql::PreparedStatement> st(_connection->prepareStatement(
        "SELECT id, account_id, character_guid, status, category, need_more_help, "
        "message, gm_response, map_id, pos_x, pos_y, pos_z, assigned_account_id, "
        "UNIX_TIMESTAMP(created_at) AS created_unix, "
        "UNIX_TIMESTAMP(updated_at) AS updated_unix "
        "FROM firelands_characters.gm_ticket "
        "WHERE assigned_account_id = ? AND status IN (0,1,2) "
        "ORDER BY updated_at DESC LIMIT ?"));
    st->setUInt(1, staffAccountId);
    st->setUInt(2, limit);
    std::unique_ptr<sql::ResultSet> rs(st->executeQuery());
    while (rs->next())
      out.push_back(RowToTicket(*rs));
  } catch (sql::SQLException &e) {
    LOG_ERROR("ListAssignedToAccount: {}", e.what());
  }
  return out;
}

std::optional<uint64_t> MySqlGmTicketRepository::Insert(const GmTicket &ticket) {
  try {
    std::shared_ptr<sql::PreparedStatement> st(_connection->prepareStatement(
        "INSERT INTO firelands_characters.gm_ticket "
        "(account_id, character_guid, status, category, need_more_help, message, "
        "gm_response, map_id, pos_x, pos_y, pos_z) "
        "VALUES (?,?,?,?,?,?,NULL,?,?,?,?)"));
    st->setUInt(1, ticket.accountId);
    st->setUInt(2, ticket.characterGuid);
    st->setUInt(3, StatusToStored(GmTicketStatus::Open));
    st->setUInt(4, ticket.category);
    st->setUInt(5, ticket.needMoreHelp ? 1u : 0u);
    st->setString(6, ticket.message);
    st->setUInt(7, ticket.mapId);
    st->setDouble(8, ticket.posX);
    st->setDouble(9, ticket.posY);
    st->setDouble(10, ticket.posZ);
    st->executeUpdate();

    std::unique_ptr<sql::Statement> idStmt(_connection->createStatement());
    std::unique_ptr<sql::ResultSet> idRs(
        idStmt->executeQuery("SELECT LAST_INSERT_ID()"));
    if (idRs->next())
      return idRs->getUInt64(1);
  } catch (sql::SQLException &e) {
    LOG_ERROR("Insert gm_ticket: {}", e.what());
  }
  return std::nullopt;
}

bool MySqlGmTicketRepository::UpdateMessage(uint64_t ticketId,
                                          uint32_t playerAccountId,
                                          std::string const &message) {
  try {
    std::shared_ptr<sql::PreparedStatement> st(_connection->prepareStatement(
        "UPDATE firelands_characters.gm_ticket SET message = ?, updated_at = NOW() "
        "WHERE id = ? AND account_id = ? AND status IN (0,1,2)"));
    st->setString(1, message);
    st->setUInt64(2, ticketId);
    st->setUInt(3, playerAccountId);
    return st->executeUpdate() > 0;
  } catch (sql::SQLException &e) {
    LOG_ERROR("UpdateMessage gm_ticket: {}", e.what());
    return false;
  }
}

bool MySqlGmTicketRepository::TryAssign(uint64_t ticketId,
                                        uint32_t staffAccountId) {
  try {
    std::shared_ptr<sql::PreparedStatement> st(_connection->prepareStatement(
        "UPDATE firelands_characters.gm_ticket "
        "SET assigned_account_id = ?, status = 1, assigned_at = NOW(), "
        "updated_at = NOW() "
        "WHERE id = ? AND status = 0 AND assigned_account_id IS NULL"));
    st->setUInt(1, staffAccountId);
    st->setUInt64(2, ticketId);
    return st->executeUpdate() > 0;
  } catch (sql::SQLException &e) {
    LOG_ERROR("TryAssign gm_ticket: {}", e.what());
    return false;
  }
}

bool MySqlGmTicketRepository::SetGmResponseAndStatus(
    uint64_t ticketId, uint32_t staffAccountId, std::string const &response,
    GmTicketStatus newStatus) {
  try {
    std::shared_ptr<sql::PreparedStatement> st(_connection->prepareStatement(
        "UPDATE firelands_characters.gm_ticket "
        "SET gm_response = ?, status = ?, updated_at = NOW() "
        "WHERE id = ? AND assigned_account_id = ? AND status IN (0,1)"));
    st->setString(1, response);
    st->setUInt(2, StatusToStored(newStatus));
    st->setUInt64(3, ticketId);
    st->setUInt(4, staffAccountId);
    return st->executeUpdate() > 0;
  } catch (sql::SQLException &e) {
    LOG_ERROR("SetGmResponseAndStatus: {}", e.what());
    return false;
  }
}

bool MySqlGmTicketRepository::CloseByPlayer(uint64_t ticketId,
                                          uint32_t playerAccountId,
                                          GmTicketStatus closedStatus) {
  try {
    std::shared_ptr<sql::PreparedStatement> st(_connection->prepareStatement(
        "UPDATE firelands_characters.gm_ticket "
        "SET status = ?, closed_at = NOW(), updated_at = NOW() "
        "WHERE id = ? AND account_id = ? AND status IN (0,1,2)"));
    st->setUInt(1, StatusToStored(closedStatus));
    st->setUInt64(2, ticketId);
    st->setUInt(3, playerAccountId);
    return st->executeUpdate() > 0;
  } catch (sql::SQLException &e) {
    LOG_ERROR("CloseByPlayer gm_ticket: {}", e.what());
    return false;
  }
}

bool MySqlGmTicketRepository::CloseByStaff(uint64_t ticketId,
                                           uint32_t staffAccountId,
                                           GmTicketStatus closedStatus) {
  try {
    std::shared_ptr<sql::PreparedStatement> st(_connection->prepareStatement(
        "UPDATE firelands_characters.gm_ticket "
        "SET status = ?, closed_at = NOW(), updated_at = NOW() "
        "WHERE id = ? AND assigned_account_id = ? AND status IN (0,1,2)"));
    st->setUInt(1, StatusToStored(closedStatus));
    st->setUInt64(2, ticketId);
    st->setUInt(3, staffAccountId);
    return st->executeUpdate() > 0;
  } catch (sql::SQLException &e) {
    LOG_ERROR("CloseByStaff gm_ticket: {}", e.what());
    return false;
  }
}

std::optional<uint64_t> MySqlGmTicketRepository::GetOldestActiveUpdatedUnix() {
  try {
    std::unique_ptr<sql::Statement> st(_connection->createStatement());
    std::unique_ptr<sql::ResultSet> rs(st->executeQuery(
        "SELECT UNIX_TIMESTAMP(MIN(updated_at)) AS ts "
        "FROM firelands_characters.gm_ticket WHERE status IN (0,1,2)"));
    if (rs->next() && !rs->isNull("ts"))
      return rs->getUInt64("ts");
  } catch (sql::SQLException &e) {
    LOG_ERROR("GetOldestActiveUpdatedUnix: {}", e.what());
  }
  return std::nullopt;
}

} // namespace Firelands
