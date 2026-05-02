#include "CommandService.h"
#include <application/ports/ICommandSession.h>
#include <shared/network/MovementInfo.h>
#include <iterator>
#include <sstream>

namespace Firelands {

CommandService::CommandService() {
  RegisterCommand("gps", {[this](auto s, auto a) { return HandleGps(s, a); },
                          ToMask(Permission::CommandGps), false});
  RegisterCommand("tele", {[this](auto s, auto a) { return HandleTele(s, a); },
                           ToMask(Permission::CommandTeleport), false});
  RegisterCommand("help", {[this](auto s, auto a) { return HandleHelp(s, a); },
                           0, false});
}

void CommandService::RegisterCommand(const std::string &name, CommandEntry entry) {
  _commands[name] = std::move(entry);
}

bool CommandService::IsCommand(const std::string &message) const {
  return !message.empty() && (message[0] == '.' || message[0] == '!');
}

bool CommandService::ExecuteCommand(std::shared_ptr<ICommandSession> session,
                                    const std::string &message,
                                    PrivilegeOrigin origin) {
  if (!session || !IsCommand(message))
    return false;

  std::string cmdStr = message.substr(1);
  std::istringstream iss(cmdStr);
  std::vector<std::string> args{std::istream_iterator<std::string>{iss},
                                std::istream_iterator<std::string>{}};

  if (args.empty())
    return false;

  std::string cmdName = args[0];
  args.erase(args.begin());

  auto it = _commands.find(cmdName);
  if (it != _commands.end()) {
    CommandEntry const &entry = it->second;
    if (entry.consoleOnly && origin != PrivilegeOrigin::ServerConsole) {
      session->SendNotification(
          "This command is only available from the server console.");
      return true;
    }
    if (!HasPermission(session->GetAccountAccessLevel(), origin,
                        entry.requiredPermissions)) {
      session->SendNotification("Insufficient privileges.");
      return true;
    }
    return entry.handler(session, args);
  }

  session->SendNotification("Unknown command: " + cmdName);
  return false;
}

bool CommandService::HandleGps(std::shared_ptr<ICommandSession> session,
                               const std::vector<std::string> &) {
  const auto &pos = session->GetPosition();
  std::string msg = "Current Position: X=" + std::to_string(pos.x) +
                    " Y=" + std::to_string(pos.y) +
                    " Z=" + std::to_string(pos.z) +
                    " O=" + std::to_string(pos.orientation);
  session->SendNotification(msg);
  return true;
}

bool CommandService::HandleTele(std::shared_ptr<ICommandSession> session,
                                const std::vector<std::string> &args) {
  try {
    if (args.size() < 3) {
      session->SendNotification("Usage: .tele <x> <y> <z> [mapId]");
      return false;
    }

    float x = std::stof(args[0]);
    float y = std::stof(args[1]);
    float z = std::stof(args[2]);
    uint32 mapId =
        (args.size() > 3) ? static_cast<uint32>(std::stoul(args[3])) : 0;

    session->TeleportTo(mapId, x, y, z);
    return true;
  } catch (const std::exception &e) {
    session->SendNotification("Error: Invalid arguments.");
    return false;
  }
}

bool CommandService::HandleHelp(std::shared_ptr<ICommandSession> session,
                                const std::vector<std::string> &) {
  // Several short lines read better in-game and in the world console.
  session->SendNotification("--- Firelands staff commands ---");
  session->SendNotification(
      "Every command starts with '.' or '!' (example: .help or !help).");
  session->SendNotification(
      ".help  Shows this help. No special rank required.");
  session->SendNotification(
      ".gps   Prints your current position (X, Y, Z, orientation). "
      "Requires moderator (or higher).");
  session->SendNotification(
      ".tele <x> <y> <z> [mapId]  Teleports your character. "
      "mapId is optional (default 0). Requires game master (or higher).");
  session->SendNotification(
      "Ranks: Player < Moderator < Game Master < Administrator. "
      "The world server console runs commands with full privileges.");
  session->SendNotification(
      "From the world console (TUI or stdin), type quit, exit, .quit, or "
      ".exit to shut down the server process.");
  return true;
}

} // namespace Firelands
