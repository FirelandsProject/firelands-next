#pragma once
#include <shared/game/Permissions.h>
#include <functional>
#include <memory>
#include <string>

namespace Firelands {
class ICommandSession;

class ICommandService {
public:
  virtual ~ICommandService() = default;
  virtual bool ExecuteCommand(std::shared_ptr<ICommandSession> session,
                              const std::string &message,
                              PrivilegeOrigin origin = PrivilegeOrigin::GameClient) = 0;
  virtual bool IsCommand(const std::string &message) const = 0;

  /// Drains scheduled restart countdown (world main / console thread).
  virtual void PollScheduledRestart() {}

  /// When the restart timer elapses, this runs on the world thread (typically
  /// requests interactive console shutdown).
  virtual void SetShutdownRequestHandler(std::function<void()> handler) {
    (void)handler;
  }
};
} // namespace Firelands
