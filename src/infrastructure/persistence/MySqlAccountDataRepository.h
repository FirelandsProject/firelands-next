#ifndef FIRELANDS_INFRASTRUCTURE_PERSISTENCE_MYSQL_ACCOUNT_DATA_REPOSITORY_H
#define FIRELANDS_INFRASTRUCTURE_PERSISTENCE_MYSQL_ACCOUNT_DATA_REPOSITORY_H

#include <shared/network/AccountDataTypes.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <conncpp.hpp>

namespace Firelands {

/**
 * Persists WoW client account-data blobs: globals in auth DB, per-character in
  * characters DB (matches reference split).
 */
class MySqlAccountDataRepository {
public:
  MySqlAccountDataRepository(std::shared_ptr<sql::Connection> authConn,
                             std::shared_ptr<sql::Connection> charConn);

  void LoadGlobal(uint32_t accountId, std::array<AccountDataSlot, 8> &slots) const;
  void LoadCharacter(uint32_t characterGuid,
                     std::array<AccountDataSlot, 8> &slots) const;

  void UpsertGlobal(uint32_t accountId, uint8_t type, uint32_t time,
                    std::string const &data) const;
  void UpsertCharacter(uint32_t characterGuid, uint8_t type, uint32_t time,
                    std::string const &data) const;

  void DeleteGlobal(uint32_t accountId, uint8_t type) const;
  void DeleteCharacter(uint32_t characterGuid, uint8_t type) const;

private:
  std::shared_ptr<sql::Connection> _authConn;
  std::shared_ptr<sql::Connection> _charConn;
};

} // namespace Firelands

#endif
