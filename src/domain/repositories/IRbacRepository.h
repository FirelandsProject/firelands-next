#ifndef FIRELANDS_DOMAIN_REPOSITORIES_IRBAC_REPOSITORY_H
#define FIRELANDS_DOMAIN_REPOSITORIES_IRBAC_REPOSITORY_H

#include <domain/models/RbacRole.h>
#include <optional>
#include <string>
#include <vector>

namespace Firelands {

class IRbacRepository {
public:
  virtual ~IRbacRepository() = default;

  virtual std::vector<RbacRole> ListRoles() = 0;
  virtual std::optional<RbacRole> FindRoleByName(std::string const &name) = 0;
  virtual std::optional<RbacRole> FindRoleById(uint32_t roleId) = 0;
  virtual std::optional<uint32_t> CreateRole(std::string const &name,
                                             uint64_t permissionMask) = 0;
  virtual bool UpdateRolePermissionMask(uint32_t roleId,
                                        uint64_t permissionMask) = 0;
  virtual bool DeleteRoleByName(std::string const &name) = 0;

  virtual std::vector<RbacRole> ListRolesForAccount(uint32_t accountId) = 0;
  virtual bool GrantRoleToAccount(uint32_t accountId, uint32_t roleId) = 0;
  virtual bool RevokeRoleFromAccount(uint32_t accountId, uint32_t roleId) = 0;
  virtual uint64_t UnionPermissionMaskForAccount(uint32_t accountId) = 0;

  /// Inserts built-in moderator / gamemaster / administrator roles if missing.
  virtual void EnsureBuiltinRoles() = 0;

  /// Replaces built-in staff role assignments (moderator, gamemaster,
  /// administrator). `roleName` empty or `none` clears staff roles.
  virtual bool SetPrimaryStaffRole(uint32_t accountId,
                                  std::string const &roleName) = 0;
};

} // namespace Firelands

#endif // FIRELANDS_DOMAIN_REPOSITORIES_IRBAC_REPOSITORY_H
