#include "MySqlRbacRepository.h"
#include <cctype>
#include <shared/game/RbacBuiltinRoles.h>
#include <shared/Logger.h>

namespace Firelands {

namespace {

bool EnsureRbacTables(std::shared_ptr<sql::Connection> conn) {
  try {
    std::unique_ptr<sql::Statement> st(conn->createStatement());
    st->execute(
        "CREATE TABLE IF NOT EXISTS `firelands_auth`.`rbac_role` ("
        "`id` int unsigned NOT NULL AUTO_INCREMENT,"
        "`name` varchar(64) NOT NULL,"
        "`permission_mask` bigint unsigned NOT NULL DEFAULT '0',"
        "PRIMARY KEY (`id`),"
        "UNIQUE KEY `uk_rbac_role_name` (`name`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
    st->execute(
        "CREATE TABLE IF NOT EXISTS `firelands_auth`.`rbac_account_role` ("
        "`account_id` int unsigned NOT NULL,"
        "`role_id` int unsigned NOT NULL,"
        "PRIMARY KEY (`account_id`, `role_id`),"
        "KEY `idx_rbac_account_role_role` (`role_id`),"
        "CONSTRAINT `fk_rbac_account_role_account` "
        "FOREIGN KEY (`account_id`) REFERENCES `account` (`id`) ON DELETE CASCADE,"
        "CONSTRAINT `fk_rbac_account_role_role` "
        "FOREIGN KEY (`role_id`) REFERENCES `rbac_role` (`id`) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
    return true;
  } catch (sql::SQLException &e) {
    if (e.getErrorCode() == 1050)
      return true;
    LOG_WARN("EnsureRbacTables failed: {}", e.what());
    return false;
  }
}

std::string NormalizeRoleName(std::string const &name) {
  std::string out;
  out.reserve(name.size());
  for (unsigned char c : name) {
    if (std::isspace(c))
      continue;
    out.push_back(static_cast<char>(std::tolower(c)));
  }
  return out;
}

} // namespace

MySqlRbacRepository::MySqlRbacRepository(
    std::shared_ptr<sql::Connection> connection)
    : _connection(std::move(connection)) {
  EnsureRbacTables(_connection);
  EnsureBuiltinRoles();
  MigrateLegacyAccessLevelsToRoles();
}

RbacRole MySqlRbacRepository::RowToRole(sql::ResultSet &rs) {
  RbacRole role;
  role.id = static_cast<uint32_t>(rs.getUInt("id"));
  role.name = rs.getString("name");
  role.permissionMask = static_cast<uint64_t>(rs.getUInt64("permission_mask"));
  return role;
}

std::vector<RbacRole> MySqlRbacRepository::ListRoles() {
  std::vector<RbacRole> out;
  try {
    std::unique_ptr<sql::Statement> st(_connection->createStatement());
    std::unique_ptr<sql::ResultSet> rs(st->executeQuery(
        "SELECT id, name, permission_mask FROM rbac_role ORDER BY name"));
    while (rs->next())
      out.push_back(RowToRole(*rs));
  } catch (sql::SQLException &e) {
    LOG_ERROR("MySqlRbacRepository::ListRoles: {}", e.what());
  }
  return out;
}

std::optional<RbacRole>
MySqlRbacRepository::FindRoleByName(std::string const &name) {
  std::string const key = NormalizeRoleName(name);
  if (key.empty())
    return std::nullopt;
  try {
    std::shared_ptr<sql::PreparedStatement> ps(_connection->prepareStatement(
        "SELECT id, name, permission_mask FROM rbac_role WHERE name = ? LIMIT 1"));
    ps->setString(1, key);
    std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
    if (rs->next())
      return RowToRole(*rs);
  } catch (sql::SQLException &e) {
    LOG_ERROR("MySqlRbacRepository::FindRoleByName: {}", e.what());
  }
  return std::nullopt;
}

std::optional<RbacRole> MySqlRbacRepository::FindRoleById(uint32_t roleId) {
  try {
    std::shared_ptr<sql::PreparedStatement> ps(_connection->prepareStatement(
        "SELECT id, name, permission_mask FROM rbac_role WHERE id = ? LIMIT 1"));
    ps->setUInt(1, roleId);
    std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
    if (rs->next())
      return RowToRole(*rs);
  } catch (sql::SQLException &e) {
    LOG_ERROR("MySqlRbacRepository::FindRoleById: {}", e.what());
  }
  return std::nullopt;
}

std::optional<uint32_t>
MySqlRbacRepository::CreateRole(std::string const &name, uint64_t permissionMask) {
  std::string const key = NormalizeRoleName(name);
  if (key.empty())
    return std::nullopt;
  try {
    std::shared_ptr<sql::PreparedStatement> ps(_connection->prepareStatement(
        "INSERT INTO rbac_role (name, permission_mask) VALUES (?, ?)"));
    ps->setString(1, key);
    ps->setUInt64(2, permissionMask);
    ps->executeUpdate();

    std::unique_ptr<sql::Statement> idStmt(_connection->createStatement());
    std::unique_ptr<sql::ResultSet> idRs(
        idStmt->executeQuery("SELECT LAST_INSERT_ID()"));
    if (idRs->next())
      return static_cast<uint32_t>(idRs->getUInt64(1));
  } catch (sql::SQLException &e) {
    LOG_ERROR("MySqlRbacRepository::CreateRole: {}", e.what());
  }
  return std::nullopt;
}

bool MySqlRbacRepository::UpdateRolePermissionMask(uint32_t roleId,
                                                   uint64_t permissionMask) {
  try {
    std::shared_ptr<sql::PreparedStatement> ps(_connection->prepareStatement(
        "UPDATE rbac_role SET permission_mask = ? WHERE id = ?"));
    ps->setUInt64(1, permissionMask);
    ps->setUInt(2, roleId);
    return ps->executeUpdate() > 0;
  } catch (sql::SQLException &e) {
    LOG_ERROR("MySqlRbacRepository::UpdateRolePermissionMask: {}", e.what());
    return false;
  }
}

bool MySqlRbacRepository::DeleteRoleByName(std::string const &name) {
  std::string const key = NormalizeRoleName(name);
  if (key.empty())
    return false;
  try {
    std::shared_ptr<sql::PreparedStatement> ps(_connection->prepareStatement(
        "DELETE FROM rbac_role WHERE name = ?"));
    ps->setString(1, key);
    return ps->executeUpdate() > 0;
  } catch (sql::SQLException &e) {
    LOG_ERROR("MySqlRbacRepository::DeleteRoleByName: {}", e.what());
    return false;
  }
}

std::vector<RbacRole>
MySqlRbacRepository::ListRolesForAccount(uint32_t accountId) {
  std::vector<RbacRole> out;
  try {
    std::shared_ptr<sql::PreparedStatement> ps(_connection->prepareStatement(
        "SELECT r.id, r.name, r.permission_mask "
        "FROM rbac_role r "
        "INNER JOIN rbac_account_role ar ON ar.role_id = r.id "
        "WHERE ar.account_id = ? "
        "ORDER BY r.name"));
    ps->setUInt(1, accountId);
    std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
    while (rs->next())
      out.push_back(RowToRole(*rs));
  } catch (sql::SQLException &e) {
    LOG_ERROR("MySqlRbacRepository::ListRolesForAccount: {}", e.what());
  }
  return out;
}

bool MySqlRbacRepository::GrantRoleToAccount(uint32_t accountId,
                                             uint32_t roleId) {
  try {
    std::shared_ptr<sql::PreparedStatement> ps(_connection->prepareStatement(
        "INSERT IGNORE INTO rbac_account_role (account_id, role_id) "
        "VALUES (?, ?)"));
    ps->setUInt(1, accountId);
    ps->setUInt(2, roleId);
    ps->executeUpdate();
    return true;
  } catch (sql::SQLException &e) {
    LOG_ERROR("MySqlRbacRepository::GrantRoleToAccount: {}", e.what());
    return false;
  }
}

bool MySqlRbacRepository::RevokeRoleFromAccount(uint32_t accountId,
                                                uint32_t roleId) {
  try {
    std::shared_ptr<sql::PreparedStatement> ps(_connection->prepareStatement(
        "DELETE FROM rbac_account_role WHERE account_id = ? AND role_id = ?"));
    ps->setUInt(1, accountId);
    ps->setUInt(2, roleId);
    return ps->executeUpdate() > 0;
  } catch (sql::SQLException &e) {
    LOG_ERROR("MySqlRbacRepository::RevokeRoleFromAccount: {}", e.what());
    return false;
  }
}

void MySqlRbacRepository::EnsureBuiltinRoles() {
  for (auto const &[name, mask] : BuiltinStaffRoles()) {
    if (FindRoleByName(std::string(name)))
      continue;
    CreateRole(std::string(name), mask);
  }
}

bool MySqlRbacRepository::SetPrimaryStaffRole(uint32_t accountId,
                                              std::string const &roleName) {
  std::string const key = NormalizeRoleName(roleName);
  for (auto const &[builtin, _] : BuiltinStaffRoles()) {
    auto role = FindRoleByName(std::string(builtin));
    if (role)
      RevokeRoleFromAccount(accountId, role->id);
  }
  if (key.empty() || key == "none" || key == "player")
    return true;
  auto role = FindRoleByName(key);
  if (!role)
    return false;
  return GrantRoleToAccount(accountId, role->id);
}

void MySqlRbacRepository::MigrateLegacyAccessLevelsToRoles() {
  try {
    std::unique_ptr<sql::Statement> st(_connection->createStatement());
    st->execute(
        "INSERT IGNORE INTO rbac_account_role (account_id, role_id) "
        "SELECT a.id, r.id FROM account a "
        "INNER JOIN rbac_role r ON r.name = CASE a.access_level "
        "WHEN 1 THEN 'moderator' WHEN 2 THEN 'gamemaster' "
        "WHEN 3 THEN 'administrator' ELSE '' END "
        "WHERE a.access_level > 0");
    st->execute("UPDATE account SET access_level = 0 WHERE access_level > 0");
  } catch (sql::SQLException &e) {
    LOG_WARN("MigrateLegacyAccessLevelsToRoles: {}", e.what());
  }
}

uint64_t MySqlRbacRepository::UnionPermissionMaskForAccount(uint32_t accountId) {
  uint64_t mask = 0;
  try {
    std::shared_ptr<sql::PreparedStatement> ps(_connection->prepareStatement(
        "SELECT COALESCE(BIT_OR(r.permission_mask), 0) AS mask "
        "FROM rbac_account_role ar "
        "INNER JOIN rbac_role r ON r.id = ar.role_id "
        "WHERE ar.account_id = ?"));
    ps->setUInt(1, accountId);
    std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
    if (rs->next())
      mask = static_cast<uint64_t>(rs->getUInt64("mask"));
  } catch (sql::SQLException &e) {
    LOG_ERROR("MySqlRbacRepository::UnionPermissionMaskForAccount: {}", e.what());
  }
  return mask;
}

} // namespace Firelands
