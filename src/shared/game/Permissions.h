#ifndef FIRELANDS_SHARED_GAME_PERMISSIONS_H
#define FIRELANDS_SHARED_GAME_PERMISSIONS_H

#include <shared/game/AccessLevel.h>
#include <cstdint>

namespace Firelands {

using PermissionMask = uint64_t;

/// Fine-grained capabilities granted by default per `AccessLevel` (see
/// `DefaultPermissions`). Extend the enum as new staff features appear.
enum class Permission : uint64_t {
  CommandGps = 1ull << 0,
  CommandTeleport = 1ull << 1,
  ModerateChat = 1ull << 2,
  ManagePlayers = 1ull << 3,
  ManageAccounts = 1ull << 4,
  ServerControl = 1ull << 5,
  /// In-game GM appearance / movement helpers (`.gm`, `.visible`, `.fly`, `.speed`).
  CommandGmTools = 1ull << 6,
  /// Learn spell, money, items, level (`.learn`, `.money`, `.additem`, `.delitem`,
  /// `.level`).
  CommandGameplay = 1ull << 7,
  /// GM help ticket queue (`.ticket …`).
  ManageGmTickets = 1ull << 8,
  /// Open mailbox UI anywhere (`.email` — moderator+).
  CommandMailbox = 1ull << 9,
};

inline constexpr PermissionMask ToMask(Permission p) {
  return static_cast<PermissionMask>(p);
}

inline PermissionMask DefaultPermissions(AccessLevel level) {
  switch (level) {
  case AccessLevel::Player:
    return 0;
  case AccessLevel::Moderator:
    return ToMask(Permission::ModerateChat) | ToMask(Permission::CommandGps) |
           ToMask(Permission::CommandMailbox);
  case AccessLevel::GameMaster:
    return DefaultPermissions(AccessLevel::Moderator) |
           ToMask(Permission::CommandTeleport) |
           ToMask(Permission::ManagePlayers) |
           ToMask(Permission::CommandGmTools) |
           ToMask(Permission::CommandGameplay) |
           ToMask(Permission::ManageGmTickets);
  case AccessLevel::Administrator:
    return DefaultPermissions(AccessLevel::GameMaster) |
           ToMask(Permission::ManageAccounts) | ToMask(Permission::ServerControl);
  case AccessLevel::Console:
    return UINT64_MAX;
  }
  return 0;
}

/// Effective grant for command checks: RBAC role mask in-game; all bits on console.
inline PermissionMask EffectivePermissions(PrivilegeOrigin origin,
                                           PermissionMask roleMask) {
  if (origin == PrivilegeOrigin::ServerConsole)
    return UINT64_MAX;
  return roleMask;
}

/// `required == 0` means any caller that passed staff command gates may run it.
/// `roleMask` is the OR of `rbac_role.permission_mask` for the account.
inline bool HasPermission(PrivilegeOrigin origin, PermissionMask required,
                          PermissionMask roleMask) {
  if (required == 0)
    return true;
  PermissionMask const granted = EffectivePermissions(origin, roleMask);
  return (granted & required) == required;
}

inline bool CanUseModeratorDotCommands(PermissionMask roleMask) {
  return (roleMask & DefaultPermissions(AccessLevel::Moderator)) != 0;
}

inline bool CanUseGameMasterDotCommands(PermissionMask roleMask) {
  return HasPermission(PrivilegeOrigin::GameClient,
                       DefaultPermissions(AccessLevel::GameMaster), roleMask);
}

} // namespace Firelands

#endif // FIRELANDS_SHARED_GAME_PERMISSIONS_H
