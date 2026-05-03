#include "MySqlAccountRepository.h"
#include <conncpp.hpp>
#include <shared/Crypto.h>
#include <shared/game/AccessLevel.h>
#include <shared/Logger.h>
#include <sstream>

namespace Firelands {

namespace {

bool EnsureAccountLockedColumn(std::shared_ptr<sql::Connection> conn) {
  try {
    std::unique_ptr<sql::Statement> st(conn->createStatement());
    st->execute(
        "ALTER TABLE `firelands_auth`.`account` "
        "ADD COLUMN `locked` tinyint unsigned NOT NULL DEFAULT 0 "
        "AFTER `expansion`");
    LOG_DEBUG("Added missing column `firelands_auth.account.locked`.");
    return true;
  } catch (sql::SQLException &e) {
    if (e.getErrorCode() == 1060)
      return true;
    std::string const msg{e.what()};
    if (msg.find("Duplicate column") != std::string::npos)
      return true;
    LOG_WARN("EnsureAccountLockedColumn failed: {}", e.what());
    return false;
  }
}

} // namespace

MySqlAccountRepository::MySqlAccountRepository(
    std::shared_ptr<sql::Connection> connection)
    : _connection(std::move(connection)) {
  EnsureAccountLockedColumn(_connection);
}

std::optional<Account>
MySqlAccountRepository::FindByUsername(const std::string &username) {
  try {
    std::shared_ptr<sql::PreparedStatement> stmnt(_connection->prepareStatement(
        "SELECT id, username, email, salt, verifier, expansion, access_level, "
        "locked "
        "FROM account "
        "WHERE username = ?"));
    stmnt->setString(1, username);

    std::unique_ptr<sql::ResultSet> res(stmnt->executeQuery());

    if (res->next()) {
      Account acc;
      acc.id = res->getInt("id");
      acc.username = res->getString("username");
      acc.email = res->getString("email");

      // Read Binary Salt
      auto saltStream = res->getBlob("salt");
      acc.salt.assign(std::istreambuf_iterator<char>(*saltStream),
                      std::istreambuf_iterator<char>());

      // Read Binary Verifier
      auto verifierStream = res->getBlob("verifier");
      acc.verifier.assign(std::istreambuf_iterator<char>(*verifierStream),
                          std::istreambuf_iterator<char>());

      acc.expansion = static_cast<uint8>(res->getInt("expansion"));
      acc.accessLevel =
          AccessLevelFromStored(static_cast<uint8>(res->getInt("access_level")));
      acc.locked = res->getUInt("locked") != 0;
      return acc;
    }
  } catch (sql::SQLException &e) {
    LOG_ERROR("Database error in FindByUsername: {}", e.what());
    return std::nullopt;
  }

  return std::nullopt;
}

void MySqlAccountRepository::Create(const Account &account) {
  try {
    std::shared_ptr<sql::PreparedStatement> stmnt(_connection->prepareStatement(
        "INSERT INTO account (username, email, salt, verifier, expansion, "
        "access_level) "
        "VALUES (?, ?, ?, ?, ?, ?)"));
    stmnt->setString(1, account.username);
    stmnt->setString(2, account.email);

    // Set Salt as Blob
    std::string saltStr(account.salt.begin(), account.salt.end());
    std::istringstream saltStream(saltStr);
    stmnt->setBlob(3, &saltStream);

    // Set Verifier as Blob
    std::string verifierStr(account.verifier.begin(), account.verifier.end());
    std::istringstream verifierStream(verifierStr);
    stmnt->setBlob(4, &verifierStream);

    stmnt->setInt(5, account.expansion);
    stmnt->setInt(6, AccessLevelToStored(account.accessLevel));
    stmnt->executeUpdate();
  } catch (sql::SQLException &e) {
    LOG_ERROR("Database error in Create: {}", e.what());
  }
}

void MySqlAccountRepository::Update(const Account &account) {
  try {
    std::shared_ptr<sql::PreparedStatement> stmnt(_connection->prepareStatement(
        "UPDATE account SET email = ?, expansion = ?, access_level = ? "
        "WHERE id = ?"));
    stmnt->setString(1, account.email);
    stmnt->setInt(2, account.expansion);
    stmnt->setInt(3, AccessLevelToStored(account.accessLevel));
    stmnt->setInt(4, account.id);
    stmnt->executeUpdate();
  } catch (sql::SQLException &e) {
    LOG_ERROR("Database error in Update: {}", e.what());
  }
}

void MySqlAccountRepository::DeleteByUsername(const std::string &username) {
  try {
    std::shared_ptr<sql::PreparedStatement> stmnt(_connection->prepareStatement(
        "DELETE FROM account WHERE username = ?"));
    stmnt->setString(1, username);
    stmnt->executeUpdate();
  } catch (sql::SQLException &e) {
    LOG_ERROR("Database error in DeleteByUsername: {}", e.what());
  }
}

void MySqlAccountRepository::CreateSession(
    uint32 accountId, const std::vector<uint8_t> &sessionKey) {
  try {
    // Using REPLACE INTO to handle existing sessions for the same account
    std::shared_ptr<sql::PreparedStatement> stmnt(_connection->prepareStatement(
        "REPLACE INTO account_session (id, session_key) VALUES (?, ?)"));
    stmnt->setUInt(1, accountId);

    std::string keyStr(sessionKey.begin(), sessionKey.end());
    std::istringstream keyStream(keyStr);
    stmnt->setBlob(2, &keyStream);

    stmnt->executeUpdate();
  } catch (sql::SQLException &e) {
    LOG_ERROR("Database error in CreateSession: {}", e.what());
  }
}

std::vector<uint8_t> MySqlAccountRepository::GetSessionKey(uint32 accountId) {
  std::vector<uint8_t> key;
  try {
    std::shared_ptr<sql::PreparedStatement> stmnt(_connection->prepareStatement(
        "SELECT session_key FROM account_session WHERE id = ?"));
    stmnt->setUInt(1, accountId);

    std::unique_ptr<sql::ResultSet> res(stmnt->executeQuery());
    if (res->next()) {
      auto keyStream = res->getBlob("session_key");
      std::string rawData;
      rawData.assign(std::istreambuf_iterator<char>(*keyStream),
                     std::istreambuf_iterator<char>());

      // If it looks like a hex string (usually 80 chars for 40-byte key),
      // convert it.
      if (rawData.length() == 80) {
        key = Crypto::FromHexString(rawData);
      } else {
        key.assign(rawData.begin(), rawData.end());
      }
    }
  } catch (sql::SQLException &e) {
    LOG_ERROR("Database error in GetSessionKey: {}", e.what());
  }
  return key;
}

void MySqlAccountRepository::SetLockedByUsername(const std::string &usernameUpper,
                                                bool locked) {
  try {
    std::shared_ptr<sql::PreparedStatement> st(_connection->prepareStatement(
        "UPDATE account SET locked = ? WHERE username = ?"));
    st->setUInt(1, locked ? 1u : 0u);
    st->setString(2, usernameUpper);
    st->executeUpdate();
  } catch (sql::SQLException &e) {
    LOG_ERROR("SetLockedByUsername failed: {}", e.what());
  }
}

} // namespace Firelands
