#ifndef FIRELANDS_SHARED_GAME_ACCESS_LEVEL_H
#define FIRELANDS_SHARED_GAME_ACCESS_LEVEL_H

#include <cstdint>

namespace Firelands {

/// Legacy numeric staff tiers (0 = player … 3 = administrator). Staff capabilities
/// are granted via RBAC roles (`rbac_role` / `rbac_account_role`); this enum
/// remains for `realmlist.allowedSecurityLevel` and migration helpers only.
/// `Console` is never persisted: full grants come from `PrivilegeOrigin::ServerConsole`.
enum class AccessLevel : uint8_t {
  Player = 0,
  Moderator = 1,
  GameMaster = 2,
  Administrator = 3,
  Console = 4,
};

enum class PrivilegeOrigin : uint8_t {
  GameClient = 0,
  ServerConsole = 1,
};

inline AccessLevel AccessLevelFromStored(uint8_t stored) {
  if (stored > static_cast<uint8_t>(AccessLevel::Administrator))
    return AccessLevel::Administrator;
  return static_cast<AccessLevel>(stored);
}

inline uint8_t AccessLevelToStored(AccessLevel level) {
  if (static_cast<uint8_t>(level) >
      static_cast<uint8_t>(AccessLevel::Administrator))
    return static_cast<uint8_t>(AccessLevel::Player);
  return static_cast<uint8_t>(level);
}

} // namespace Firelands

#endif // FIRELANDS_SHARED_GAME_ACCESS_LEVEL_H
