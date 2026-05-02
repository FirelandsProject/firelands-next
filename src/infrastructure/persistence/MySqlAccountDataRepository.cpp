#include "MySqlAccountDataRepository.h"
#include <shared/Logger.h>
#include <shared/network/AccountDataTypes.h>
#include <sstream>

namespace Firelands {

namespace {

bool IsMissingTableSqlError(sql::SQLException &e) {
  return e.getErrorCode() == 1146;
}

std::string ReadBlobAsString(sql::ResultSet &rs, char const *column) {
  auto stream = rs.getBlob(column);
  if (!stream)
    return {};
  return std::string(std::istreambuf_iterator<char>(*stream),
                     std::istreambuf_iterator<char>());
}

} // namespace

MySqlAccountDataRepository::MySqlAccountDataRepository(
    std::shared_ptr<sql::Connection> authConn,
    std::shared_ptr<sql::Connection> charConn)
    : _authConn(std::move(authConn)), _charConn(std::move(charConn)) {}

void MySqlAccountDataRepository::LoadGlobal(
    uint32_t accountId, std::array<AccountDataSlot, 8> &slots) const {
  for (uint32_t t = 0; t < 8; ++t) {
    if (IsGlobalAccountDataType(t))
      slots[t] = {};
  }
  if (!_authConn)
    return;
  try {
    auto stmnt(_authConn->prepareStatement(
        "SELECT `type`, `time`, `data` FROM account_data WHERE accountId = ?"));
    stmnt->setUInt(1, accountId);
    std::unique_ptr<sql::ResultSet> res(stmnt->executeQuery());
    while (res->next()) {
      uint32_t const type = static_cast<uint32_t>(res->getUInt("type"));
      if (type >= 8 || !IsGlobalAccountDataType(type))
        continue;
      slots[type].time = static_cast<uint32_t>(res->getUInt("time"));
      slots[type].data = ReadBlobAsString(*res, "data");
    }
  } catch (sql::SQLException &e) {
    LOG_ERROR("LoadGlobal account_data: {}", e.what());
  }
}

void MySqlAccountDataRepository::LoadCharacter(
    uint32_t characterGuid, std::array<AccountDataSlot, 8> &slots) const {
  for (uint32_t t = 0; t < 8; ++t) {
    if (IsPerCharacterAccountDataType(t))
      slots[t] = {};
  }
  if (!_charConn)
    return;
  try {
    auto stmnt(_charConn->prepareStatement(
        "SELECT `type`, `time`, `data` FROM character_account_data WHERE guid = "
        "?"));
    stmnt->setUInt(1, characterGuid);
    std::unique_ptr<sql::ResultSet> res(stmnt->executeQuery());
    while (res->next()) {
      uint32_t const type = static_cast<uint32_t>(res->getUInt("type"));
      if (type >= 8 || !IsPerCharacterAccountDataType(type))
        continue;
      slots[type].time = static_cast<uint32_t>(res->getUInt("time"));
      slots[type].data = ReadBlobAsString(*res, "data");
    }
  } catch (sql::SQLException &e) {
    if (IsMissingTableSqlError(e)) {
      LOG_WARN(
          "character_account_data table missing (run world migrations / apply "
          "sql/z_ensure_character_account_data.sql): {}",
          e.what());
    } else {
      LOG_ERROR("LoadCharacter character_account_data: {}", e.what());
    }
  }
}

void MySqlAccountDataRepository::UpsertGlobal(uint32_t accountId, uint8_t type,
                                            uint32_t time,
                                            std::string const &data) const {
  if (!_authConn || !IsGlobalAccountDataType(type))
    return;
  try {
    auto stmnt(_authConn->prepareStatement(
        "REPLACE INTO account_data (accountId, type, time, data) VALUES (?, ?, "
        "?, ?)"));
    stmnt->setUInt(1, accountId);
    stmnt->setUInt(2, type);
    stmnt->setUInt(3, time);
    // istream must stay alive until executeUpdate finishes (connector reads blob
    // synchronously from the stream).
    std::istringstream blobStream(data);
    // Two-arg setBlob can mis-read binary payloads (bindings/macros); length is required.
    stmnt->setBlob(4, &blobStream, static_cast<int64_t>(data.size()));
    stmnt->executeUpdate();
  } catch (sql::SQLException &e) {
    LOG_ERROR("UpsertGlobal account_data: {}", e.what());
  }
}

void MySqlAccountDataRepository::UpsertCharacter(uint32_t characterGuid,
                                               uint8_t type, uint32_t time,
                                               std::string const &data) const {
  if (!_charConn || !IsPerCharacterAccountDataType(type))
    return;
  try {
    auto stmnt(_charConn->prepareStatement(
        "REPLACE INTO character_account_data (guid, type, time, data) VALUES (?, "
        "?, ?, ?)"));
    stmnt->setUInt(1, characterGuid);
    stmnt->setUInt(2, type);
    stmnt->setUInt(3, time);
    std::istringstream blobStream(data);
    stmnt->setBlob(4, &blobStream, static_cast<int64_t>(data.size()));
    stmnt->executeUpdate();
  } catch (sql::SQLException &e) {
    if (IsMissingTableSqlError(e)) {
      LOG_WARN(
          "character_account_data missing; bindings/macros not saved until "
          "sql/z_ensure_character_account_data.sql is applied: {}",
          e.what());
    } else {
      LOG_ERROR("UpsertCharacter character_account_data: {}", e.what());
    }
  }
}

void MySqlAccountDataRepository::DeleteGlobal(uint32_t accountId,
                                              uint8_t type) const {
  if (!_authConn || !IsGlobalAccountDataType(type))
    return;
  try {
    auto stmnt(_authConn->prepareStatement(
        "DELETE FROM account_data WHERE accountId = ? AND type = ?"));
    stmnt->setUInt(1, accountId);
    stmnt->setUInt(2, type);
    stmnt->executeUpdate();
  } catch (sql::SQLException &e) {
    LOG_ERROR("DeleteGlobal account_data: {}", e.what());
  }
}

void MySqlAccountDataRepository::DeleteCharacter(uint32_t characterGuid,
                                                 uint8_t type) const {
  if (!_charConn || !IsPerCharacterAccountDataType(type))
    return;
  try {
    auto stmnt(_charConn->prepareStatement(
        "DELETE FROM character_account_data WHERE guid = ? AND type = ?"));
    stmnt->setUInt(1, characterGuid);
    stmnt->setUInt(2, type);
    stmnt->executeUpdate();
  } catch (sql::SQLException &e) {
    if (IsMissingTableSqlError(e)) {
      LOG_WARN("DeleteCharacter character_account_data (table missing): {}",
               e.what());
    } else {
      LOG_ERROR("DeleteCharacter character_account_data: {}", e.what());
    }
  }
}

} // namespace Firelands
