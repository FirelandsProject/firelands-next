#ifndef FIRELANDS_SHARED_GAME_ACCESS_LEVEL_H
#define FIRELANDS_SHARED_GAME_ACCESS_LEVEL_H

#include <cstdint>

namespace Firelands {

/// Stored on `account.access_level` (0 = player … 3 = administrator).
/// `Console` is never persisted: it applies only when the server runs an action
/// from its own terminal / REPL (`PrivilegeOrigin::ServerConsole`).
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

inline AccessLevel EffectiveAccess(AccessLevel accountLevel,
                                   PrivilegeOrigin origin) {
  return (origin == PrivilegeOrigin::ServerConsole) ? AccessLevel::Console
                                                     : accountLevel;
}

inline bool HasAtLeast(AccessLevel actor, AccessLevel required) {
  return static_cast<uint8_t>(actor) >= static_cast<uint8_t>(required);
}

} // namespace Firelands

#endif // FIRELANDS_SHARED_GAME_ACCESS_LEVEL_H
