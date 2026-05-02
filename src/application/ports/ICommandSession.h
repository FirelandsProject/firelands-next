#pragma once

#include <shared/game/AccessLevel.h>
#include <cstdint>
#include <string>

namespace Firelands {

struct MovementInfo;

/// Minimal surface used by `ICommandService` / `CommandService` (in-game client or
/// server REPL).
class ICommandSession {
public:
  virtual ~ICommandSession() = default;
  virtual void SendNotification(const std::string &message) = 0;
  virtual const MovementInfo &GetPosition() const = 0;
  virtual void TeleportTo(uint32_t mapId, float x, float y, float z,
                          float orientation = 0.0f) = 0;
  virtual AccessLevel GetAccountAccessLevel() const = 0;
};

} // namespace Firelands
