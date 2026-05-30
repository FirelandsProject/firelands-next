#pragma once
#include <application/ports/ICommandService.h>
#include <shared/game/AccessLevel.h>
#include <shared/game/Permissions.h>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Firelands {

class ICommandSession;
class IAccountRepository;
class OnlineCharacterSessionRegistry;
class CharacterService;
class GmTicketService;

enum class ConsoleArgLayout {
  /// Same argument order as in-game (e.g. `.tele x y z`).
  SameAsInGame = 0,
  /// From `PrivilegeOrigin::ServerConsole`, first arg is online character name,
  /// then the same arguments as in-game (e.g. `.tele Name x y z`).
  TargetOnlineCharacterFirst = 1,
};

/// Where a registered dot-command may run (`ExecuteCommand` filters on this
/// after lookup, before permissions). Maps to BOTH / GAME / CONSOLE in docs.
enum class CommandAvailability {
  Both,
  /// In-game world client only (e.g. needs persisted `account.id` on session).
  Game,
  /// `PrivilegeOrigin::ServerConsole` only (stdin / TUI REPL).
  Console,
};

class CommandService : public ICommandService {
public:
  CommandService(
      std::shared_ptr<OnlineCharacterSessionRegistry> onlineCharacters = {},
      std::shared_ptr<IAccountRepository> accountRepo = {},
      std::shared_ptr<CharacterService> characterService = {},
      std::shared_ptr<GmTicketService> gmTicketService = {});

  bool ExecuteCommand(std::shared_ptr<ICommandSession> session,
                      const std::string &message,
                      PrivilegeOrigin origin = PrivilegeOrigin::GameClient) override;
  bool IsCommand(const std::string &message) const override;
  void PollScheduledRestart() override;
  void SetShutdownRequestHandler(std::function<void()> handler) override;

private:
  using CommandHandler =
      std::function<bool(std::shared_ptr<ICommandSession>,
                         const std::vector<std::string> &, PrivilegeOrigin)>;

  struct CommandEntry {
    CommandHandler handler;
    PermissionMask requiredPermissions = 0;
    CommandAvailability availability = CommandAvailability::Both;
    ConsoleArgLayout consoleLayout = ConsoleArgLayout::SameAsInGame;
  };

  void RegisterCommand(const std::string &name, CommandEntry entry);

  bool HandleGps(std::shared_ptr<ICommandSession> session,
                 const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleMmap(std::shared_ptr<ICommandSession> session,
                  const std::vector<std::string> &args, PrivilegeOrigin origin);
  void ClearMmapMarkers(std::shared_ptr<ICommandSession> session,
                        uint64_t playerGuid, uint32_t mapId);
  bool HandleTele(std::shared_ptr<ICommandSession> session,
                  const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleHelp(std::shared_ptr<ICommandSession> session,
                  const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleAccount(std::shared_ptr<ICommandSession> session,
                     const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleGmTag(std::shared_ptr<ICommandSession> session,
                   const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleDndTag(std::shared_ptr<ICommandSession> session,
                    const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleDevTag(std::shared_ptr<ICommandSession> session,
                    const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleGmVisible(std::shared_ptr<ICommandSession> session,
                       const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleGmFly(std::shared_ptr<ICommandSession> session,
                   const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleGmSpeed(std::shared_ptr<ICommandSession> session,
                     const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleOnline(std::shared_ptr<ICommandSession> session,
                    const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleAnnounce(std::shared_ptr<ICommandSession> session,
                      const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleKick(std::shared_ptr<ICommandSession> session,
                  const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleGoto(std::shared_ptr<ICommandSession> session,
                  const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleSummon(std::shared_ptr<ICommandSession> session,
                    const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleLearn(std::shared_ptr<ICommandSession> session,
                   const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleUnlearn(std::shared_ptr<ICommandSession> session,
                     const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleMoney(std::shared_ptr<ICommandSession> session,
                    const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleAdditem(std::shared_ptr<ICommandSession> session,
                      const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleDelitem(std::shared_ptr<ICommandSession> session,
                     const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleLevel(std::shared_ptr<ICommandSession> session,
                   const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleCd(std::shared_ptr<ICommandSession> session,
                const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleDamage(std::shared_ptr<ICommandSession> session,
                    const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleRevive(std::shared_ptr<ICommandSession> session,
                    const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleBan(std::shared_ptr<ICommandSession> session,
                  const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleUnban(std::shared_ptr<ICommandSession> session,
                   const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleTicket(std::shared_ptr<ICommandSession> session,
                    const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleServer(std::shared_ptr<ICommandSession> session,
                    const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleEmail(std::shared_ptr<ICommandSession> session,
                   const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleNpc(std::shared_ptr<ICommandSession> session,
                 const std::vector<std::string> &args, PrivilegeOrigin origin);
  bool HandleFaction(std::shared_ptr<ICommandSession> session,
                     const std::vector<std::string> &args, PrivilegeOrigin origin);

  std::shared_ptr<OnlineCharacterSessionRegistry> _onlineCharacters;
  std::shared_ptr<IAccountRepository> _accountRepo;
  std::shared_ptr<CharacterService> _characterService;
  std::shared_ptr<GmTicketService> _gmTicketService;
  std::map<std::string, CommandEntry> _commands;
  std::unordered_map<uint64_t, std::vector<std::pair<uint64_t, std::chrono::steady_clock::time_point>>> _mmapMarkers;

  std::function<void()> _shutdownRequestHandler;
  std::optional<std::chrono::steady_clock::time_point> _restartDeadline;
  /// Highest whole-second "remaining" value already communicated (initial
  /// announcement uses the total delay; countdown covers 10…1 without duplicating
  /// the top second when total ≤ 10).
  int _restartAnnouncedDownTo = 0;
};

} // namespace Firelands
