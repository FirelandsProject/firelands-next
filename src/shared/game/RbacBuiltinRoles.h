#ifndef FIRELANDS_SHARED_GAME_RBAC_BUILTIN_ROLES_H
#define FIRELANDS_SHARED_GAME_RBAC_BUILTIN_ROLES_H

#include <shared/game/AccessLevel.h>
#include <shared/game/Permissions.h>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace Firelands {

inline constexpr std::string_view kBuiltinRoleModerator = "moderator";
inline constexpr std::string_view kBuiltinRoleGameMaster = "gamemaster";
inline constexpr std::string_view kBuiltinRoleAdministrator = "administrator";

/// Built-in staff roles that replace legacy `account.access_level` tiers.
inline std::vector<std::pair<std::string_view, PermissionMask>> const &
BuiltinStaffRoles() {
  static std::vector<std::pair<std::string_view, PermissionMask>> const kRoles = {
      {kBuiltinRoleModerator, DefaultPermissions(AccessLevel::Moderator)},
      {kBuiltinRoleGameMaster, DefaultPermissions(AccessLevel::GameMaster)},
      {kBuiltinRoleAdministrator,
       DefaultPermissions(AccessLevel::Administrator)},
  };
  return kRoles;
}

/// Maps legacy `access_level` (1–3) to a built-in role name; 0 = no staff role.
inline std::optional<std::string_view>
BuiltinRoleNameForLegacyAccessLevel(uint8_t stored) {
  switch (AccessLevelFromStored(stored)) {
  case AccessLevel::Moderator:
    return kBuiltinRoleModerator;
  case AccessLevel::GameMaster:
    return kBuiltinRoleGameMaster;
  case AccessLevel::Administrator:
    return kBuiltinRoleAdministrator;
  default:
    return std::nullopt;
  }
}

/// Highest legacy tier rank implied by an effective permission mask (0–3).
inline int EffectiveStaffTierRank(PermissionMask mask) {
  if ((mask & DefaultPermissions(AccessLevel::Administrator)) ==
      DefaultPermissions(AccessLevel::Administrator))
    return static_cast<int>(AccessLevel::Administrator);
  if ((mask & DefaultPermissions(AccessLevel::GameMaster)) ==
      DefaultPermissions(AccessLevel::GameMaster))
    return static_cast<int>(AccessLevel::GameMaster);
  if ((mask & DefaultPermissions(AccessLevel::Moderator)) ==
      DefaultPermissions(AccessLevel::Moderator))
    return static_cast<int>(AccessLevel::Moderator);
  return static_cast<int>(AccessLevel::Player);
}

/// Realm `allowedSecurityLevel` gate (same numeric scale as legacy tiers).
inline bool MeetsRealmSecurityRequirement(PermissionMask mask,
                                          uint8_t allowedSecurityLevelStored) {
  if (allowedSecurityLevelStored == 0)
    return true;
  return EffectiveStaffTierRank(mask) >=
         static_cast<int>(allowedSecurityLevelStored);
}

} // namespace Firelands

#endif // FIRELANDS_SHARED_GAME_RBAC_BUILTIN_ROLES_H
