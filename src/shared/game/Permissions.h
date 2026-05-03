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
};

inline constexpr PermissionMask ToMask(Permission p) {
  return static_cast<PermissionMask>(p);
}

inline PermissionMask DefaultPermissions(AccessLevel level) {
  switch (level) {
  case AccessLevel::Player:
    return 0;
  case AccessLevel::Moderator:
    return ToMask(Permission::ModerateChat) | ToMask(Permission::CommandGps);
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

/// `required == 0` means any authenticated game client (or console) may run the
/// command (subject to `CommandAvailability` in `CommandService`).
inline bool HasPermission(AccessLevel account, PrivilegeOrigin origin,
                          PermissionMask required) {
  if (required == 0)
    return true;
  AccessLevel const effective = EffectiveAccess(account, origin);
  PermissionMask const granted = DefaultPermissions(effective);
  return (granted & required) == required;
}

} // namespace Firelands

#endif // FIRELANDS_SHARED_GAME_PERMISSIONS_H
