#pragma once
#include <application/ports/ICommandService.h>
#include <shared/game/Permissions.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Firelands {

class ICommandSession;

class CommandService : public ICommandService {
public:
  CommandService();
  bool ExecuteCommand(std::shared_ptr<ICommandSession> session,
                      const std::string &message,
                      PrivilegeOrigin origin = PrivilegeOrigin::GameClient) override;
  bool IsCommand(const std::string &message) const override;

private:
  using CommandHandler = std::function<bool(std::shared_ptr<ICommandSession>,
                                            const std::vector<std::string> &)>;

  struct CommandEntry {
    CommandHandler handler;
    PermissionMask requiredPermissions = 0;
    bool consoleOnly = false;
  };

  void RegisterCommand(const std::string &name, CommandEntry entry);

  // Handlers
  bool HandleGps(std::shared_ptr<ICommandSession> session,
                 const std::vector<std::string> &args);
  bool HandleTele(std::shared_ptr<ICommandSession> session,
                  const std::vector<std::string> &args);
  bool HandleHelp(std::shared_ptr<ICommandSession> session,
                  const std::vector<std::string> &args);

  std::map<std::string, CommandEntry> _commands;
};

} // namespace Firelands
