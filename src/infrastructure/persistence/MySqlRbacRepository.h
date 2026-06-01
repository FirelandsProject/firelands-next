#pragma once

#include <domain/repositories/IRbacRepository.h>
#include <conncpp.hpp>
#include <memory>

namespace Firelands {

class MySqlRbacRepository final : public IRbacRepository {
public:
  explicit MySqlRbacRepository(std::shared_ptr<sql::Connection> connection);

  std::vector<RbacRole> ListRoles() override;
  std::optional<RbacRole> FindRoleByName(std::string const &name) override;
  std::optional<RbacRole> FindRoleById(uint32_t roleId) override;
  std::optional<uint32_t> CreateRole(std::string const &name,
                                     uint64_t permissionMask) override;
  bool UpdateRolePermissionMask(uint32_t roleId,
                              uint64_t permissionMask) override;
  bool DeleteRoleByName(std::string const &name) override;

  std::vector<RbacRole> ListRolesForAccount(uint32_t accountId) override;
  bool GrantRoleToAccount(uint32_t accountId, uint32_t roleId) override;
  bool RevokeRoleFromAccount(uint32_t accountId, uint32_t roleId) override;
  uint64_t UnionPermissionMaskForAccount(uint32_t accountId) override;

  void EnsureBuiltinRoles() override;
  bool SetPrimaryStaffRole(uint32_t accountId,
                           std::string const &roleName) override;

private:
  void MigrateLegacyAccessLevelsToRoles();

  std::shared_ptr<sql::Connection> _connection;
  static RbacRole RowToRole(sql::ResultSet &rs);
};

} // namespace Firelands
