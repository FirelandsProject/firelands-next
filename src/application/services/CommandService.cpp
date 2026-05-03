#include "CommandService.h"
#include <application/ports/ICommandSession.h>
#include <application/services/GmTicketService.h>
#include <application/services/OnlineCharacterSessionRegistry.h>
#include <application/services/CharacterService.h>
#include <domain/models/Character.h>
#include <application/services/SRPService.h>
#include <domain/repositories/IAccountRepository.h>
#include <shared/Common.h>
#include <shared/Crypto.h>
#include <shared/game/AccessLevel.h>
#include <shared/game/Permissions.h>
#include <shared/Logger.h>
#include <shared/network/MovementInfo.h>
#include <cmath>
#include <cctype>
#include <chrono>
#include <cstring>
#include <iterator>
#include <limits>
#include <sstream>

namespace Firelands {

namespace {

class DelegatingCommandSession final : public ICommandSession {
  std::shared_ptr<ICommandSession> _subject;
  std::shared_ptr<ICommandSession> _operatorSession;

public:
  DelegatingCommandSession(std::shared_ptr<ICommandSession> subject,
                           std::shared_ptr<ICommandSession> operatorSession)
      : _subject(std::move(subject)),
        _operatorSession(std::move(operatorSession)) {}

  void SendNotification(const std::string &message) override {
    if (_operatorSession)
      _operatorSession->SendNotification(message);
  }

  const MovementInfo &GetPosition() const override {
    return _subject->GetPosition();
  }

  void TeleportTo(uint32_t mapId, float x, float y, float z,
                  float orientation) override {
    _subject->TeleportTo(mapId, x, y, z, orientation);
  }

  AccessLevel GetAccountAccessLevel() const override {
    return _subject->GetAccountAccessLevel();
  }

  void SetGmTagEnabled(bool on) override { _subject->SetGmTagEnabled(on); }

  void SetDndEnabled(bool on) override { _subject->SetDndEnabled(on); }

  void SetDevTagEnabled(bool on) override { _subject->SetDevTagEnabled(on); }

  void SetGmVisibleToPlayers(bool visible) override {
    _subject->SetGmVisibleToPlayers(visible);
  }

  void SetGmFlyEnabled(bool on) override { _subject->SetGmFlyEnabled(on); }

  void SetGmRunSpeed(float speed) override { _subject->SetGmRunSpeed(speed); }

  void OpenGmMailboxUi() override { _subject->OpenGmMailboxUi(); }

  uint32 GetMapId() const override { return _subject->GetMapId(); }

  void RequestDisconnect(std::string const &reason) override {
    _subject->RequestDisconnect(reason);
  }

  bool GmLearnSpell(uint32 spellId) override {
    return _subject->GmLearnSpell(spellId);
  }

  bool GmModifyMoneyCopper(int64 deltaCopper) override {
    return _subject->GmModifyMoneyCopper(deltaCopper);
  }

  bool GmAddItem(uint32 itemEntry, uint32 count) override {
    return _subject->GmAddItem(itemEntry, count);
  }

  bool GmRemoveItem(uint32 itemEntry, uint32 count) override {
    return _subject->GmRemoveItem(itemEntry, count);
  }

  bool GmSetLevel(uint8 level) override { return _subject->GmSetLevel(level); }

  uint64_t GetClientSelectionGuid() const override {
    return _operatorSession ? _operatorSession->GetClientSelectionGuid() : 0;
  }

  uint64_t GetActiveCharacterObjectGuid() const override {
    return _subject->GetActiveCharacterObjectGuid();
  }

  void SendGmResponseReceived(uint32_t ticketId, std::string const &playerMessage,
                              std::string const &gmResponse) override {
    _subject->SendGmResponseReceived(ticketId, playerMessage, gmResponse);
  }

  uint32_t GetAccountId() const override { return _subject->GetAccountId(); }
};

static std::string JoinArgs(std::vector<std::string>::const_iterator begin,
                            std::vector<std::string>::const_iterator end) {
  std::string out;
  for (auto it = begin; it != end; ++it) {
    if (!out.empty())
      out += ' ';
    out += *it;
  }
  return out;
}

static bool AsciiEqualsLower(std::string const &a, char const *b) {
  size_t const n = std::strlen(b);
  if (a.size() != n)
    return false;
  for (size_t i = 0; i < n; ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        static_cast<unsigned char>(b[i])) {
      return false;
    }
  }
  return true;
}

static bool IsAllDigitAscii(std::string const &s) {
  if (s.empty())
    return false;
  for (unsigned char c : s) {
    if (!std::isdigit(c))
      return false;
  }
  return true;
}

static bool IsWowColorHexDigit(char c) {
  unsigned char const u = static_cast<unsigned char>(c);
  return (u >= '0' && u <= '9') || (u >= 'a' && u <= 'f') ||
         (u >= 'A' && u <= 'F');
}

/// Strips WoW chat color tokens (`|cAARRGGBB` … `|r`) so server-console output is
/// readable in a plain terminal (no raw `|cff…` sequences).
static std::string StripWowChatColorTokens(std::string const &in) {
  std::string out;
  out.reserve(in.size());
  for (size_t i = 0; i < in.size();) {
    if (in[i] == '|' && i + 1 < in.size()) {
      char const op = in[i + 1];
      if ((op == 'c' || op == 'C') && i + 10 <= in.size()) {
        bool ok = true;
        for (size_t k = 0; k < 8; ++k) {
          if (!IsWowColorHexDigit(in[i + 2 + k])) {
            ok = false;
            break;
          }
        }
        if (ok) {
          i += 10;
          continue;
        }
      }
      if (op == 'r' || op == 'R') {
        i += 2;
        continue;
      }
    }
    out.push_back(in[i++]);
  }
  return out;
}

static constexpr uint64_t kMaxRestartDelaySeconds = 7ULL * 24 * 3600;

static bool ParseRestartDelayToken(std::string const &token,
                                    std::chrono::seconds &out) {
  if (token.size() < 2)
    return false;
  char const u =
      static_cast<char>(std::tolower(static_cast<unsigned char>(token.back())));
  if (u != 's' && u != 'm')
    return false;
  std::string const num = token.substr(0, token.size() - 1);
  if (num.empty())
    return false;
  for (unsigned char c : num) {
    if (!std::isdigit(c))
      return false;
  }
  uint64_t n = 0;
  try {
    n = std::stoull(num);
  } catch (...) {
    return false;
  }
  if (n == 0)
    return false;
  uint64_t seconds = 0;
  if (u == 's') {
    seconds = n;
  } else {
    if (n > kMaxRestartDelaySeconds / 60)
      return false;
    seconds = n * 60;
  }
  if (seconds > kMaxRestartDelaySeconds)
    return false;
  if (seconds > static_cast<uint64_t>(std::numeric_limits<int>::max()))
    return false;
  out = std::chrono::seconds(static_cast<int>(seconds));
  return true;
}

static int CeilSecondsRemaining(std::chrono::steady_clock::time_point now,
                                std::chrono::steady_clock::time_point deadline) {
  if (now >= deadline)
    return 0;
  auto const ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
  int64_t const c = ms.count();
  return std::max(1, static_cast<int>((c + 999) / 1000));
}

static std::string FormatRestartLeadIn(std::chrono::seconds total) {
  auto const s = static_cast<uint64_t>(total.count());
  if (s >= 60 && s % 60 == 0) {
    uint64_t const m = s / 60;
    return (m == 1) ? std::string("1 minute.")
                    : (std::to_string(m) + " minutes.");
  }
  return std::to_string(s) + ((s == 1) ? " second." : " seconds.");
}

} // namespace

CommandService::CommandService(
    std::shared_ptr<OnlineCharacterSessionRegistry> onlineCharacters,
    std::shared_ptr<IAccountRepository> accountRepo,
    std::shared_ptr<CharacterService> characterService,
    std::shared_ptr<GmTicketService> gmTicketService)
    : _onlineCharacters(std::move(onlineCharacters)),
      _accountRepo(std::move(accountRepo)),
      _characterService(std::move(characterService)),
      _gmTicketService(std::move(gmTicketService)) {
  RegisterCommand("gps", {[this](auto s, auto a, auto o) { return HandleGps(s, a, o); },
                          ToMask(Permission::CommandGps), CommandAvailability::Both,
                          ConsoleArgLayout::TargetOnlineCharacterFirst});
  RegisterCommand("tele", {[this](auto s, auto a, auto o) { return HandleTele(s, a, o); },
                           ToMask(Permission::CommandTeleport), CommandAvailability::Both,
                           ConsoleArgLayout::TargetOnlineCharacterFirst});
  RegisterCommand("help", {[this](auto s, auto a, auto o) { return HandleHelp(s, a, o); },
                           0, CommandAvailability::Both,
                           ConsoleArgLayout::SameAsInGame});
  RegisterCommand(
      "commands", {[this](auto s, auto a, auto o) { return HandleHelp(s, a, o); }, 0,
                   CommandAvailability::Both, ConsoleArgLayout::SameAsInGame});
  RegisterCommand(
      "account", {[this](auto s, auto a, auto o) { return HandleAccount(s, a, o); },
                  ToMask(Permission::ManageAccounts), CommandAvailability::Console,
                  ConsoleArgLayout::SameAsInGame});
  RegisterCommand("gm", {[this](auto s, auto a, auto o) { return HandleGmTag(s, a, o); },
                  ToMask(Permission::CommandGmTools), CommandAvailability::Both,
                  ConsoleArgLayout::SameAsInGame});
  RegisterCommand("dnd", {[this](auto s, auto a, auto o) { return HandleDndTag(s, a, o); },
                  ToMask(Permission::CommandGmTools), CommandAvailability::Both,
                  ConsoleArgLayout::SameAsInGame});
  RegisterCommand("dev", {[this](auto s, auto a, auto o) { return HandleDevTag(s, a, o); },
                  ToMask(Permission::CommandGmTools), CommandAvailability::Both,
                  ConsoleArgLayout::SameAsInGame});
  RegisterCommand(
      "visible", {[this](auto s, auto a, auto o) { return HandleGmVisible(s, a, o); },
                  ToMask(Permission::CommandGmTools), CommandAvailability::Both,
                  ConsoleArgLayout::SameAsInGame});
  RegisterCommand("fly", {[this](auto s, auto a, auto o) { return HandleGmFly(s, a, o); },
                  ToMask(Permission::CommandGmTools), CommandAvailability::Both,
                  ConsoleArgLayout::SameAsInGame});
  RegisterCommand(
      "speed", {[this](auto s, auto a, auto o) { return HandleGmSpeed(s, a, o); },
                ToMask(Permission::CommandGmTools), CommandAvailability::Both,
                ConsoleArgLayout::SameAsInGame});
  RegisterCommand(
      "online", {[this](auto s, auto a, auto o) { return HandleOnline(s, a, o); },
                 ToMask(Permission::ManagePlayers), CommandAvailability::Both,
                 ConsoleArgLayout::SameAsInGame});
  RegisterCommand(
      "who", {[this](auto s, auto a, auto o) { return HandleOnline(s, a, o); },
              ToMask(Permission::ManagePlayers), CommandAvailability::Both,
              ConsoleArgLayout::SameAsInGame});
  RegisterCommand(
      "announce", {[this](auto s, auto a, auto o) { return HandleAnnounce(s, a, o); },
                   ToMask(Permission::ManagePlayers), CommandAvailability::Both,
                   ConsoleArgLayout::SameAsInGame});
  RegisterCommand(
      "kick", {[this](auto s, auto a, auto o) { return HandleKick(s, a, o); },
               ToMask(Permission::ManagePlayers), CommandAvailability::Both,
               ConsoleArgLayout::SameAsInGame});
  RegisterCommand(
      "goto", {[this](auto s, auto a, auto o) { return HandleGoto(s, a, o); },
               ToMask(Permission::ManagePlayers), CommandAvailability::Both,
               ConsoleArgLayout::SameAsInGame});
  RegisterCommand(
      "appear", {[this](auto s, auto a, auto o) { return HandleGoto(s, a, o); },
                 ToMask(Permission::ManagePlayers), CommandAvailability::Both,
                 ConsoleArgLayout::SameAsInGame});
  RegisterCommand(
      "summon", {[this](auto s, auto a, auto o) { return HandleSummon(s, a, o); },
                 ToMask(Permission::ManagePlayers), CommandAvailability::Both,
                 ConsoleArgLayout::SameAsInGame});
  RegisterCommand(
      "learn", {[this](auto s, auto a, auto o) { return HandleLearn(s, a, o); },
                ToMask(Permission::CommandGameplay), CommandAvailability::Both,
                ConsoleArgLayout::TargetOnlineCharacterFirst});
  RegisterCommand(
      "money", {[this](auto s, auto a, auto o) { return HandleMoney(s, a, o); },
               ToMask(Permission::CommandGameplay), CommandAvailability::Both,
               ConsoleArgLayout::TargetOnlineCharacterFirst});
  RegisterCommand(
      "additem", {[this](auto s, auto a, auto o) { return HandleAdditem(s, a, o); },
                  ToMask(Permission::CommandGameplay), CommandAvailability::Both,
                  ConsoleArgLayout::TargetOnlineCharacterFirst});
  RegisterCommand(
      "delitem", {[this](auto s, auto a, auto o) { return HandleDelitem(s, a, o); },
                  ToMask(Permission::CommandGameplay), CommandAvailability::Both,
                  ConsoleArgLayout::TargetOnlineCharacterFirst});
  RegisterCommand(
      "level", {[this](auto s, auto a, auto o) { return HandleLevel(s, a, o); },
               ToMask(Permission::CommandGameplay), CommandAvailability::Both,
               ConsoleArgLayout::TargetOnlineCharacterFirst});
  RegisterCommand(
      "ban", {[this](auto s, auto a, auto o) { return HandleBan(s, a, o); },
              ToMask(Permission::ManageAccounts), CommandAvailability::Console,
              ConsoleArgLayout::SameAsInGame});
  RegisterCommand(
      "unban", {[this](auto s, auto a, auto o) { return HandleUnban(s, a, o); },
                ToMask(Permission::ManageAccounts), CommandAvailability::Console,
                ConsoleArgLayout::SameAsInGame});
  RegisterCommand(
      "ticket", {[this](auto s, auto a, auto o) { return HandleTicket(s, a, o); },
                 ToMask(Permission::ManageGmTickets), CommandAvailability::Game,
                 ConsoleArgLayout::SameAsInGame});
  RegisterCommand(
      "email", {[this](auto s, auto a, auto o) { return HandleEmail(s, a, o); },
                ToMask(Permission::CommandMailbox), CommandAvailability::Game,
                ConsoleArgLayout::SameAsInGame});
  RegisterCommand(
      "server", {[this](auto s, auto a, auto o) { return HandleServer(s, a, o); },
                 ToMask(Permission::ServerControl), CommandAvailability::Both,
                 ConsoleArgLayout::SameAsInGame});
}

void CommandService::RegisterCommand(const std::string &name, CommandEntry entry) {
  _commands[name] = std::move(entry);
}

bool CommandService::IsCommand(const std::string &message) const {
  return !message.empty() && message[0] == '.';
}

bool CommandService::ExecuteCommand(std::shared_ptr<ICommandSession> session,
                                    const std::string &message,
                                    PrivilegeOrigin origin) {
  if (!session || !IsCommand(message))
    return false;

  // In-game: dot commands are for Game Master (2+) except `.email`, which
  // moderators (1+) may use to open the mailbox UI anywhere.
  if (origin == PrivilegeOrigin::GameClient) {
    AccessLevel const acc = session->GetAccountAccessLevel();
    std::string const tail = message.substr(1);
    std::istringstream peekIss(tail);
    std::string firstToken;
    peekIss >> firstToken;
    bool const moderatorEmail =
        AsciiEqualsLower(firstToken, "email") &&
        HasAtLeast(acc, AccessLevel::Moderator);
    if (!HasAtLeast(acc, AccessLevel::GameMaster) && !moderatorEmail) {
      return true;
    }
  }

  std::string cmdStr = message.substr(1);
  std::istringstream iss(cmdStr);
  std::vector<std::string> args{std::istream_iterator<std::string>{iss},
                                 std::istream_iterator<std::string>{}};

  if (args.empty())
    return false;

  std::string cmdName = args[0];
  args.erase(args.begin());

  auto it = _commands.find(cmdName);
  if (it == _commands.end()) {
    LOG_DEBUG("Unknown command: .{} from Account={}", cmdName, session->GetAccountId());
    session->SendNotification("Unknown command: " + cmdName);
    return false;
  }

  CommandEntry const &entry = it->second;

  LOG_DEBUG("Command executed: Account={} Access={} Cmd=.{} Args={}",
            session->GetAccountId(),
            static_cast<int>(session->GetAccountAccessLevel()),
            cmdName, args.size());
  switch (entry.availability) {
  case CommandAvailability::Console:
    if (origin != PrivilegeOrigin::ServerConsole) {
      session->SendNotification(
          "This command is only available from the server console.");
      return true;
    }
    break;
  case CommandAvailability::Game:
    if (origin != PrivilegeOrigin::GameClient) {
      session->SendNotification("This command is only available in-game.");
      return true;
    }
    break;
  case CommandAvailability::Both:
    break;
  }
  if (!HasPermission(session->GetAccountAccessLevel(), origin,
                     entry.requiredPermissions)) {
    session->SendNotification("Insufficient privileges.");
    return true;
  }

  std::shared_ptr<ICommandSession> execSession = session;
  std::vector<std::string> execArgs = args;

  if (origin == PrivilegeOrigin::ServerConsole &&
      entry.consoleLayout == ConsoleArgLayout::TargetOnlineCharacterFirst) {
    if (args.empty()) {
      session->SendNotification(
          "From the console this command needs an online character name first "
          "(example: .gps CharName  or  .tele CharName x y z [mapId]).");
      return true;
    }
    if (!_onlineCharacters) {
      session->SendNotification(
          "Online character registry is not configured on this world process.");
      return true;
    }
    std::string const targetName = args[0];
    execArgs.assign(args.begin() + 1, args.end());
    auto target = _onlineCharacters->TryResolve(targetName);
    if (!target) {
      session->SendNotification(std::string("Character not online: ") + targetName);
      return true;
    }
    execSession =
        std::make_shared<DelegatingCommandSession>(std::move(target), session);
  }

  return entry.handler(execSession, execArgs, origin);
}

bool CommandService::HandleGps(std::shared_ptr<ICommandSession> session,
                               const std::vector<std::string> &,
                               PrivilegeOrigin origin) {
  (void)origin;
  const auto &pos = session->GetPosition();
  std::string msg = "Current Position: X=" + std::to_string(pos.x) +
                    " Y=" + std::to_string(pos.y) +
                    " Z=" + std::to_string(pos.z) +
                    " O=" + std::to_string(pos.orientation);
  session->SendNotification(msg);
  return true;
}

bool CommandService::HandleTele(std::shared_ptr<ICommandSession> session,
                                const std::vector<std::string> &args,
                                PrivilegeOrigin origin) {
  (void)origin;
  try {
    if (args.size() < 3) {
      session->SendNotification(
          "Usage: .tele <x> <y> <z> [mapId]  (in-game), or from console: .tele "
          "<CharName> <x> <y> <z> [mapId]");
      return false;
    }

    float x = std::stof(args[0]);
    float y = std::stof(args[1]);
    float z = std::stof(args[2]);
    uint32 mapId =
        (args.size() > 3) ? static_cast<uint32>(std::stoul(args[3])) : 0;

    session->TeleportTo(mapId, x, y, z);
    return true;
  } catch (const std::exception &) {
    session->SendNotification("Error: Invalid arguments.");
    return false;
  }
}

bool CommandService::HandleHelp(std::shared_ptr<ICommandSession> session,
                                const std::vector<std::string> &,
                                PrivilegeOrigin origin) {
  auto notifyHelp = [&](char const *wowStyledText) {
    std::string msg(wowStyledText);
    if (origin == PrivilegeOrigin::ServerConsole)
      msg = StripWowChatColorTokens(msg);
    session->SendNotification(std::move(msg));
  };

  // WoW 4.3.4: |cAARRGGBBtext|r in system messages for readability.
  notifyHelp(
      "|cffFCE566=== Firelands — staff command help ===|r\n"
      "|cffAAAAAAEvery command starts with|r |cffffffff.|r\n"
      "|cffAAAAAAIn-game,|r |cffffcc00Moderator+|r |cffAAAAAAcan use|r "
      "|cffCCCCCC.email|r|cffAAAAAA; full command set from|r |cffffcc00Game Master "
      "(2+)|r|cffAAAAAA. Players see no response.|r");

  notifyHelp(
      "|cffFFD200· Help & position|r\n"
      "|cffCCCCCC.help|r |cff888888—|r Show this guide.  |cff666666e.g.|r "
      "|cffffffff.help|r\n"
      "|cffCCCCCC.commands|r |cff888888—|r Same as .help.  |cff666666e.g.|r "
      "|cffffffff.commands|r\n"
      "|cffCCCCCC.email|r |cff888888—|r |cffAAAAAA(Moderator+)|r Open mailbox UI "
      "without a mailbox NPC.  |cff666666e.g.|r |cffffffff.email|r\n"
      "|cffCCCCCC.gps|r |cff888888—|r Print your X, Y, Z, facing.  "
      "|cff666666e.g.|r |cffffffff.gps|r\n"
      "|cffCCCCCC.tele|r |cff888888—|r Teleport to coordinates.  "
      "|cff666666In-game:|r |cffffffff.tele -8759 544 97|r |cff666666(map 0 "
      "default).|r\n"
      "|cff666666With map id:|r |cffffffff.tele 100 -50 25 571|r\n"
      "|cff666666World console (target online character first):|r\n"
      "|cffffffff.tele Annabell -8759 544 97|r  |cff666666or|r  "
      "|cffffffff.tele Annabell -8759 544 97 0|r\n"
      "|cff666666Console GPS on another player:|r |cffffffff.gps Annabell|r\n"
      "|cffCCCCCC.online|r |cff888888/|r |cffCCCCCC.who|r |cff888888—|r List "
      "online character names.  |cff666666e.g.|r |cffffffff.online|r\n"
      "|cffCCCCCC.announce|r |cff888888—|r Server-wide system message.  "
      "|cff666666e.g.|r |cffffffff.announce Welcome everyone|r\n"
      "|cffCCCCCC.kick|r |cff888888—|r Disconnect an online character.  "
      "|cff666666e.g.|r |cffffffff.kick BadActor exploiting|r\n"
      "|cffCCCCCC.goto|r |cff888888/|r |cffCCCCCC.appear|r |cff888888—|r "
      "Teleport to an online player.  |cff666666Console:|r "
      "|cffffffff.goto StaffName TargetName|r\n"
      "|cffCCCCCC.summon|r |cff888888—|r Bring an online player to you.  "
      "|cff666666Console:|r |cffffffff.summon VictimName AnchorName|r\n"
      "|cffFFD200· GM tickets|r |cff666666(in-game only)|r\n"
      "|cffCCCCCC.ticket queue|r |cff888888—|r Unassigned queue.  "
      "|cff666666e.g.|r |cffffffff.ticket queue|r\n"
      "|cffCCCCCC.ticket mine|r |cff888888—|r Assigned to you.  "
      "|cff666666e.g.|r |cffffffff.ticket mine|r\n"
      "|cffCCCCCC.ticket take|r |cff888888—|r |cff666666e.g.|r "
      "|cffffffff.ticket take 1|r\n"
      "|cffCCCCCC.ticket reply|r |cff888888—|r |cff666666e.g.|r "
      "|cffffffff.ticket reply 1 Thanks for waiting.|r\n"
      "|cffCCCCCC.ticket close|r |cff888888—|r Staff-close.  "
      "|cff666666e.g.|r |cffffffff.ticket close 1|r\n"
      "|cffFFD200· Gameplay (GM)|r\n"
      "|cffCCCCCC.learn|r |cff888888—|r Learn spell by id (persists).  "
      "|cff666666Console:|r |cffffffff.learn CharName 475|r\n"
      "|cffCCCCCC.money|r |cff888888—|r Add/remove copper (signed).  "
      "|cff666666Console:|r |cffffffff.money CharName 50000|r\n"
      "|cffCCCCCC.level|r |cff888888—|r Set level 1–85.  "
      "|cff666666Console:|r |cffffffff.level Char 60|r");

  notifyHelp(
      "|cffFFD200· Items (GM)|r |cffAAAAAA—|r |cffCCCCCC.additem|r |cff888888/|r "
      "|cffCCCCCC.delitem|r\n"
      "|cffAAAAAAWho receives the command:|r |cff666666In-game:|r select another "
      "player (target) and use a |cffffffffnumeric|r first argument → item goes to "
      "that player (|cffffffff.additem 6948 1|r). If the first token is |cffffffffnot|r "
      "all digits, it is an |cffffffffonline character name|r: "
      "|cffffffff.additem Annabell 6948 5|r\n"
      "|cff666666World console:|r character name always first: "
      "|cffffffff.additem Annabell 6948 1|r\n"
      "|cffAAAAAAFull main backpack|r |cff666666(slots 23–38)|r |cffAAAAAA→|r "
      "item is stored in |cffCCCCCCmail|r |cffAAAAAA(DB); use|r |cffCCCCCC.email|r "
      "|cffAAAAAAto open the mailbox and review.|r\n"
      "|cffCCCCCC.delitem|r |cff888888—|r Same targeting as |cffCCCCCC.additem|r; "
      "removes up to |cffffffffcount|r from the |cff666666main backpack only|r "
      "(not equipped).  |cff666666e.g.|r |cffffffff.delitem 6948|r |cff666666or|r "
      "|cffffffff.delitem Annabell 6948 10|r |cff666666or|r "
      "|cffffffff.delitem 6948 1|r |cff666666with target selected.|r");

  notifyHelp(
      "|cffFFD200· Tags & visibility|r\n"
      "|cffCCCCCC.gm on|r |cff888888/|r |cffCCCCCC.gm off|r |cff888888—|r GM chat "
      "tag & object flag.  |cff666666e.g.|r |cffffffff.gm on|r\n"
      "|cffCCCCCC.dnd on|r |cff888888/|r |cffCCCCCC.dnd off|r |cff888888—|r Do "
      "Not Disturb tag.  |cff666666e.g.|r |cffffffff.dnd on|r\n"
      "|cffCCCCCC.dev on|r |cff888888/|r |cffCCCCCC.dev off|r |cff888888—|r "
      "Developer tag.  |cff666666e.g.|r |cffffffff.dev on|r\n"
      "|cffCCCCCC.visible on|r |cff888888/|r |cffCCCCCC.visible off|r "
      "|cff888888—|r |cffAAAAAAon|r = others see you; |cffAAAAAAoff|r = hidden.  "
      "|cff666666e.g.|r |cffffffff.visible off|r");

  notifyHelp(
      "|cffFFD200· Movement|r\n"
      "|cffCCCCCC.fly on|r |cff888888/|r |cffCCCCCC.fly off|r |cff888888—|r "
      "Toggle flight (client).  |cff666666e.g.|r |cffffffff.fly on|r\n"
      "|cffCCCCCC.speed|r |cff888888—|r Set run and flight speed (default 7, "
      "clamped ~0.5–50).  |cff666666e.g.|r |cffffffff.speed 12|r  |cff666666or|r  "
      "|cffffffff.speed reset|r");

  notifyHelp(
      "|cffFFD200· World console only|r\n"
      "|cffAAAAAA(Type these in the world server terminal, not in-game chat.)|r\n"
      "|cffCCCCCC.account create|r |cff888888—|r Create an auth DB account.  "
      "|cff666666e.g.|r\n"
      "|cffffffff.account create PlayerOne MyPass mail@x.com 3 2|r\n"
      "|cff666666(expansion 0–3, access 0=player … 3=admin; optional.)|r\n"
      "|cffCCCCCC.account delete|r |cff888888—|r Remove an auth account "
      "(destructive).  |cff666666e.g.|r |cffffffff.account delete PLAYERONE|r\n"
      "|cffCCCCCC.account setaccess|r |cff888888—|r Change stored GM tier; "
      "re-login to apply.  |cff666666e.g.|r\n"
      "|cffffffff.account setaccess PLAYERONE 2|r\n"
      "|cffCCCCCC.ban|r |cff888888/|r |cffCCCCCC.unban|r |cff888888—|r Lock or "
      "unlock auth login for an account.  |cff666666e.g.|r |cffffffff.ban "
      "PLAYERONE|r\n"
      "|cffCCCCCC.server restart|r |cff888888—|r Scheduled shutdown/restart "
      "after a delay (|cffffffff30s|r, |cffffffff5m|r, ...). Last 10 seconds: "
      "one |cffFFCC00[Server]|r countdown line per second.  "
      "|cff666666Administrator (or world console).|r\n"
      "|cffAAAAAAShutdown the world process:|r |cffffffffquit|r |cff888888or|r "
      "|cffffffffexit|r |cffAAAAAA(no leading dot).|r");

  return true;
}

bool CommandService::HandleAccount(std::shared_ptr<ICommandSession> session,
                                   const std::vector<std::string> &args,
                                   PrivilegeOrigin origin) {
  (void)origin;
  if (!_accountRepo) {
    session->SendNotification("Account repository is not configured.");
    return true;
  }
  if (args.empty()) {
    session->SendNotification(
        "Usage: .account create <user> <pass> [email] [expansion] [access] | "
        ".account setaccess <username> <0-3> | .account delete <username>");
    return false;
  }

  if (AsciiEqualsLower(args[0], "delete")) {
    if (args.size() < 2) {
      session->SendNotification("Usage: .account delete <username>");
      return false;
    }
    std::string const userUpper = Crypto::ToUpper(args[1]);
    if (!_accountRepo->FindByUsername(userUpper)) {
      session->SendNotification("Unknown account: " + userUpper);
      return true;
    }
    _accountRepo->DeleteByUsername(userUpper);
    session->SendNotification("Deleted account: " + userUpper);
    return true;
  }

  if (AsciiEqualsLower(args[0], "create")) {
    if (args.size() < 3) {
      session->SendNotification(
          "Usage: .account create <username> <password> [email] [expansion 0-3] "
          "[access_level 0-3]");
      return false;
    }
    std::string const userUpper = Crypto::ToUpper(args[1]);
    std::string const password = args[2];
    std::string const email =
        (args.size() >= 4) ? args[3] : (args[1] + "@firelands.com");
    uint8 expansion = 3;
    AccessLevel access = AccessLevel::Player;
    try {
      if (args.size() >= 5)
        expansion = static_cast<uint8>(std::stoul(args[4]));
      if (args.size() >= 6) {
        uint8 raw = static_cast<uint8>(std::stoul(args[5]));
        if (raw > static_cast<uint8>(AccessLevel::Administrator)) {
          session->SendNotification("access_level must be 0-3.");
          return false;
        }
        access = static_cast<AccessLevel>(raw);
      }
    } catch (const std::exception &) {
      session->SendNotification("Invalid numeric argument (expansion or access).");
      return false;
    }
    if (expansion > 3u) {
      session->SendNotification("expansion must be 0-3.");
      return false;
    }

    if (_accountRepo->FindByUsername(userUpper)) {
      session->SendNotification("Account already exists: " + userUpper);
      return true;
    }

    SRPData srp = SRPService::GenerateVerifier(args[1], password);
    Account acc;
    acc.username = userUpper;
    acc.email = email;
    acc.salt = std::move(srp.salt);
    acc.verifier = std::move(srp.verifier);
    acc.expansion = expansion;
    acc.accessLevel = access;
    _accountRepo->Create(acc);
    session->SendNotification("Account created: " + userUpper +
                              " access_level=" +
                              std::to_string(static_cast<int>(access)));
    return true;
  }

  if (AsciiEqualsLower(args[0], "setaccess")) {
    if (args.size() < 3) {
      session->SendNotification(
          "Usage: .account setaccess <username> <0-3>  (auth username, upper "
          "stored)");
      return false;
    }
    std::string const userUpper = Crypto::ToUpper(args[1]);
    AccessLevel newLevel = AccessLevel::Player;
    try {
      uint8 raw = static_cast<uint8>(std::stoul(args[2]));
      if (raw > static_cast<uint8>(AccessLevel::Administrator)) {
        session->SendNotification("Level must be 0-3.");
        return false;
      }
      newLevel = static_cast<AccessLevel>(raw);
    } catch (const std::exception &) {
      session->SendNotification("Invalid level (expected 0-3).");
      return false;
    }

    auto existing = _accountRepo->FindByUsername(userUpper);
    if (!existing) {
      session->SendNotification("Unknown account: " + userUpper);
      return true;
    }
    Account updated = *existing;
    updated.accessLevel = newLevel;
    _accountRepo->Update(updated);
    session->SendNotification("Updated " + userUpper +
                              " access_level=" +
                              std::to_string(static_cast<int>(newLevel)) +
                              " (re-login to refresh session privileges).");
    return true;
  }

  session->SendNotification(
      "Unknown .account subcommand (use create, setaccess, or delete).");
  return false;
}

bool CommandService::HandleGmTag(std::shared_ptr<ICommandSession> session,
                                 const std::vector<std::string> &args,
                                 PrivilegeOrigin origin) {
  (void)origin;
  if (args.empty()) {
    session->SendNotification("Usage: .gm on | .gm off");
    return false;
  }
  if (AsciiEqualsLower(args[0], "on")) {
    session->SetGmTagEnabled(true);
    session->SendNotification("GM tag enabled.");
    return true;
  }
  if (AsciiEqualsLower(args[0], "off")) {
    session->SetGmTagEnabled(false);
    session->SendNotification("GM tag disabled.");
    return true;
  }
  session->SendNotification("Usage: .gm on | .gm off");
  return false;
}

bool CommandService::HandleDndTag(std::shared_ptr<ICommandSession> session,
                                  const std::vector<std::string> &args,
                                  PrivilegeOrigin origin) {
  (void)origin;
  if (args.empty()) {
    session->SendNotification("Usage: .dnd on | .dnd off");
    return false;
  }
  if (AsciiEqualsLower(args[0], "on")) {
    session->SetDndEnabled(true);
    session->SendNotification("DND tag enabled.");
    return true;
  }
  if (AsciiEqualsLower(args[0], "off")) {
    session->SetDndEnabled(false);
    session->SendNotification("DND tag disabled.");
    return true;
  }
  session->SendNotification("Usage: .dnd on | .dnd off");
  return false;
}

bool CommandService::HandleDevTag(std::shared_ptr<ICommandSession> session,
                                  const std::vector<std::string> &args,
                                  PrivilegeOrigin origin) {
  (void)origin;
  if (args.empty()) {
    session->SendNotification("Usage: .dev on | .dev off");
    return false;
  }
  if (AsciiEqualsLower(args[0], "on")) {
    session->SetDevTagEnabled(true);
    session->SendNotification("Developer tag enabled.");
    return true;
  }
  if (AsciiEqualsLower(args[0], "off")) {
    session->SetDevTagEnabled(false);
    session->SendNotification("Developer tag disabled.");
    return true;
  }
  session->SendNotification("Usage: .dev on | .dev off");
  return false;
}

bool CommandService::HandleGmVisible(std::shared_ptr<ICommandSession> session,
                                     const std::vector<std::string> &args,
                                     PrivilegeOrigin origin) {
  (void)origin;
  if (args.empty()) {
    session->SendNotification("Usage: .visible on | .visible off  "
                              "(on = visible to others, off = hidden)");
    return false;
  }
  if (AsciiEqualsLower(args[0], "on")) {
    session->SetGmVisibleToPlayers(true);
    session->SendNotification("You are visible to other players.");
    return true;
  }
  if (AsciiEqualsLower(args[0], "off")) {
    session->SetGmVisibleToPlayers(false);
    session->SendNotification("You are hidden from other players (invisible).");
    return true;
  }
  session->SendNotification("Usage: .visible on | .visible off");
  return false;
}

bool CommandService::HandleGmFly(std::shared_ptr<ICommandSession> session,
                                 const std::vector<std::string> &args,
                                 PrivilegeOrigin origin) {
  (void)origin;
  if (args.empty()) {
    session->SendNotification("Usage: .fly on | .fly off");
    return false;
  }
  if (AsciiEqualsLower(args[0], "on")) {
    session->SetGmFlyEnabled(true);
    session->SendNotification("Flight enabled (client).");
    return true;
  }
  if (AsciiEqualsLower(args[0], "off")) {
    session->SetGmFlyEnabled(false);
    session->SendNotification("Flight disabled.");
    return true;
  }
  session->SendNotification("Usage: .fly on | .fly off");
  return false;
}

bool CommandService::HandleGmSpeed(std::shared_ptr<ICommandSession> session,
                                   const std::vector<std::string> &args,
                                   PrivilegeOrigin origin) {
  (void)origin;
  if (args.empty()) {
    session->SendNotification(
        "Usage: .speed <number>  or  .speed reset  (default run/flight speed 7)");
    return false;
  }
  if (AsciiEqualsLower(args[0], "reset")) {
    session->SetGmRunSpeed(7.0f);
    session->SendNotification("Run and flight speed reset to 7.");
    return true;
  }
  try {
    float v = std::stof(args[0]);
    if (!std::isfinite(v)) {
      session->SendNotification("Invalid speed.");
      return false;
    }
    session->SetGmRunSpeed(v);
    session->SendNotification("Run and flight speed set to " + std::to_string(v) +
                              ".");
    return true;
  } catch (const std::exception &) {
    session->SendNotification("Invalid speed (expected a number or reset).");
    return false;
  }
}

bool CommandService::HandleOnline(std::shared_ptr<ICommandSession> session,
                                    const std::vector<std::string> &args,
                                    PrivilegeOrigin origin) {
  (void)args;
  (void)origin;
  if (!_onlineCharacters) {
    session->SendNotification("Online registry is not configured.");
    return true;
  }
  std::vector<std::string> names = _onlineCharacters->ListOnlineCharacterNames();
  if (names.empty()) {
    session->SendNotification("No characters currently listed online.");
    return true;
  }
  std::string body = JoinArgs(names.begin(), names.end());
  std::string full =
      "|cffCCCCCCOnline (" + std::to_string(names.size()) + "):|r " + body;
  constexpr size_t kMax = 450;
  if (full.size() > kMax) {
    full.resize(kMax - 3);
    full += "...";
  }
  session->SendNotification(full);
  return true;
}

bool CommandService::HandleAnnounce(std::shared_ptr<ICommandSession> session,
                                    const std::vector<std::string> &args,
                                    PrivilegeOrigin origin) {
  (void)origin;
  if (!_onlineCharacters) {
    session->SendNotification("Online registry is not configured.");
    return true;
  }
  if (args.empty()) {
    session->SendNotification(
        "Usage: .announce <message>  (system message to every online character)");
    return false;
  }
  constexpr size_t kMax = 480;
  std::string msg = JoinArgs(args.begin(), args.end());
  if (msg.size() > kMax)
    msg.resize(kMax);
  _onlineCharacters->BroadcastAnnouncement("|cffFFCC00[Server]|r " + msg, msg);
  session->SendNotification("Announcement sent.");
  return true;
}

bool CommandService::HandleKick(std::shared_ptr<ICommandSession> session,
                                const std::vector<std::string> &args,
                                PrivilegeOrigin origin) {
  (void)origin;
  if (!_onlineCharacters) {
    session->SendNotification("Online registry is not configured.");
    return true;
  }
  if (args.empty()) {
    session->SendNotification(
        "Usage: .kick <CharacterName> [optional reason text]");
    return false;
  }
  std::string const targetName = args[0];
  auto target = _onlineCharacters->TryResolve(targetName);
  if (!target) {
    session->SendNotification(std::string("Character not online: ") + targetName);
    return true;
  }
  if (target.get() == session.get()) {
    session->SendNotification("You cannot kick yourself.");
    return true;
  }
  std::string const reason = JoinArgs(args.begin() + 1, args.end());
  target->RequestDisconnect(reason);
  session->SendNotification(std::string("Kicked: ") + targetName);
  return true;
}

bool CommandService::HandleGoto(std::shared_ptr<ICommandSession> session,
                                const std::vector<std::string> &args,
                                PrivilegeOrigin origin) {
  if (!_onlineCharacters) {
    session->SendNotification("Online registry is not configured.");
    return true;
  }
  if (origin == PrivilegeOrigin::ServerConsole) {
    if (args.size() < 2) {
      session->SendNotification(
          "Usage (console): .goto <Who> <Target>  — teleport Who to Target's "
          "position (both online).");
      return false;
    }
    auto who = _onlineCharacters->TryResolve(args[0]);
    auto target = _onlineCharacters->TryResolve(args[1]);
    if (!who) {
      session->SendNotification(std::string("Character not online: ") + args[0]);
      return true;
    }
    if (!target) {
      session->SendNotification(std::string("Character not online: ") + args[1]);
      return true;
    }
    MovementInfo const &p = target->GetPosition();
    who->TeleportTo(target->GetMapId(), p.x, p.y, p.z, p.orientation);
    session->SendNotification("Teleported " + args[0] + " to " + args[1] + ".");
    return true;
  }
  if (args.empty()) {
    session->SendNotification(
        "Usage: .goto <CharacterName>  (teleport you to an online character; "
        "alias: .appear)");
    return false;
  }
  auto target = _onlineCharacters->TryResolve(args[0]);
  if (!target) {
    session->SendNotification(std::string("Character not online: ") + args[0]);
    return true;
  }
  if (target.get() == session.get()) {
    session->SendNotification("That is your own character.");
    return true;
  }
  MovementInfo const &p = target->GetPosition();
  session->TeleportTo(target->GetMapId(), p.x, p.y, p.z, p.orientation);
  return true;
}

bool CommandService::HandleSummon(std::shared_ptr<ICommandSession> session,
                                  const std::vector<std::string> &args,
                                  PrivilegeOrigin origin) {
  if (!_onlineCharacters) {
    session->SendNotification("Online registry is not configured.");
    return true;
  }
  if (origin == PrivilegeOrigin::ServerConsole) {
    if (args.size() < 2) {
      session->SendNotification(
          "Usage (console): .summon <Who> <Anchor>  — move Who to Anchor's "
          "position (both online).");
      return false;
    }
    auto who = _onlineCharacters->TryResolve(args[0]);
    auto anchor = _onlineCharacters->TryResolve(args[1]);
    if (!who) {
      session->SendNotification(std::string("Character not online: ") + args[0]);
      return true;
    }
    if (!anchor) {
      session->SendNotification(std::string("Character not online: ") + args[1]);
      return true;
    }
    MovementInfo const &p = anchor->GetPosition();
    who->TeleportTo(anchor->GetMapId(), p.x, p.y, p.z, p.orientation);
    session->SendNotification("Summoned " + args[0] + " to " + args[1] + ".");
    return true;
  }
  if (args.empty()) {
    session->SendNotification(
        "Usage: .summon <CharacterName>  (bring an online character to you)");
    return false;
  }
  auto guest = _onlineCharacters->TryResolve(args[0]);
  if (!guest) {
    session->SendNotification(std::string("Character not online: ") + args[0]);
    return true;
  }
  if (guest.get() == session.get()) {
    session->SendNotification("That is your own character.");
    return true;
  }
  MovementInfo const &p = session->GetPosition();
  guest->TeleportTo(session->GetMapId(), p.x, p.y, p.z, p.orientation);
  session->SendNotification(std::string("Summoned: ") + args[0]);
  return true;
}

bool CommandService::HandleLearn(std::shared_ptr<ICommandSession> session,
                                 const std::vector<std::string> &args,
                                 PrivilegeOrigin origin) {
  (void)origin;
  if (args.empty()) {
    session->SendNotification(
        "Usage: .learn <spellId>  (world console: .learn <CharName> <spellId>)");
    return false;
  }
  try {
    uint32 const sid = static_cast<uint32>(std::stoul(args[0]));
    if (!session->GmLearnSpell(sid)) {
      session->SendNotification("Learn failed.");
      return false;
    }
    return true;
  } catch (const std::exception &) {
    session->SendNotification("Invalid spell id.");
    return false;
  }
}

bool CommandService::HandleMoney(std::shared_ptr<ICommandSession> session,
                                 const std::vector<std::string> &args,
                                 PrivilegeOrigin origin) {
  (void)origin;
  if (args.empty()) {
    session->SendNotification(
        "Usage: .money <deltaCopper>  (negative removes; console: .money <CharName> "
        "<delta>)");
    return false;
  }
  try {
    int64 const d = static_cast<int64>(std::stoll(args[0]));
    if (!session->GmModifyMoneyCopper(d)) {
      session->SendNotification("Money change failed.");
      return false;
    }
    return true;
  } catch (const std::exception &) {
    session->SendNotification("Invalid amount (expected integer copper delta).");
    return false;
  }
}

bool CommandService::HandleAdditem(std::shared_ptr<ICommandSession> session,
                                   const std::vector<std::string> &args,
                                   PrivilegeOrigin origin) {
  std::vector<std::string> itemArgs = args;
  std::shared_ptr<ICommandSession> subjectFromNameOrTarget;

  if (origin == PrivilegeOrigin::GameClient && _onlineCharacters) {
    if (!args.empty() && !IsAllDigitAscii(args[0])) {
      subjectFromNameOrTarget = _onlineCharacters->TryResolve(args[0]);
      if (!subjectFromNameOrTarget) {
        session->SendNotification(std::string("Character not online: ") + args[0]);
        return true;
      }
      itemArgs.assign(args.begin() + 1, args.end());
    } else if (!args.empty() && IsAllDigitAscii(args[0])) {
      uint64_t const sel = session->GetClientSelectionGuid();
      if (sel != 0) {
        if (auto peer = _onlineCharacters->TryResolveByObjectGuid(sel)) {
          if (peer.get() != session.get())
            subjectFromNameOrTarget = std::move(peer);
        }
      }
    }
  }

  std::shared_ptr<ICommandSession> exec = session;
  if (subjectFromNameOrTarget) {
    exec = std::make_shared<DelegatingCommandSession>(std::move(subjectFromNameOrTarget),
                                                    session);
  }

  if (itemArgs.empty()) {
    session->SendNotification(
        "Usage: .additem <itemId> [count]  — with another player targeted, OR "
        ".additem <OnlineCharName> <itemId> [count]\n"
        "Console: .additem <CharName> <itemId> [count]  (full bags → mail)");
    return false;
  }
  try {
    uint32 const entry = static_cast<uint32>(std::stoul(itemArgs[0]));
    uint32 count = 1;
    if (itemArgs.size() >= 2)
      count = static_cast<uint32>(std::stoul(itemArgs[1]));
    if (_characterService && !_characterService->HasItemTemplate(entry)) {
      session->SendNotification("Item does not exist (no template for entry " +
                                std::to_string(entry) + ").");
      return false;
    }
    if (!exec->GmAddItem(entry, count)) {
      session->SendNotification("Add item failed.");
      return false;
    }
    return true;
  } catch (const std::exception &) {
    session->SendNotification("Invalid item id or count.");
    return false;
  }
}

bool CommandService::HandleDelitem(std::shared_ptr<ICommandSession> session,
                                   const std::vector<std::string> &args,
                                   PrivilegeOrigin origin) {
  std::vector<std::string> itemArgs = args;
  std::shared_ptr<ICommandSession> subjectFromNameOrTarget;

  if (origin == PrivilegeOrigin::GameClient && _onlineCharacters) {
    if (!args.empty() && !IsAllDigitAscii(args[0])) {
      subjectFromNameOrTarget = _onlineCharacters->TryResolve(args[0]);
      if (!subjectFromNameOrTarget) {
        session->SendNotification(std::string("Character not online: ") + args[0]);
        return true;
      }
      itemArgs.assign(args.begin() + 1, args.end());
    } else if (!args.empty() && IsAllDigitAscii(args[0])) {
      uint64_t const sel = session->GetClientSelectionGuid();
      if (sel != 0) {
        if (auto peer = _onlineCharacters->TryResolveByObjectGuid(sel)) {
          if (peer.get() != session.get())
            subjectFromNameOrTarget = std::move(peer);
        }
      }
    }
  }

  std::shared_ptr<ICommandSession> exec = session;
  if (subjectFromNameOrTarget) {
    exec = std::make_shared<DelegatingCommandSession>(std::move(subjectFromNameOrTarget),
                                                    session);
  }

  if (itemArgs.empty()) {
    session->SendNotification(
        "Usage: .delitem <itemId> [count]  — with another player targeted, OR "
        ".delitem <OnlineCharName> <itemId> [count]\n"
        "Console: .delitem <CharName> <itemId> [count]  (main backpack only)");
    return false;
  }
  try {
    uint32 const entry = static_cast<uint32>(std::stoul(itemArgs[0]));
    uint32 count = 1;
    if (itemArgs.size() >= 2)
      count = static_cast<uint32>(std::stoul(itemArgs[1]));
    if (!exec->GmRemoveItem(entry, count)) {
      session->SendNotification("Remove item failed (no matching backpack stacks).");
      return false;
    }
    return true;
  } catch (const std::exception &) {
    session->SendNotification("Invalid item id or count.");
    return false;
  }
}

bool CommandService::HandleLevel(std::shared_ptr<ICommandSession> session,
                                 const std::vector<std::string> &args,
                                 PrivilegeOrigin origin) {
  (void)origin;
  if (args.empty()) {
    session->SendNotification(
        "Usage: .level <1-85>  (console: .level <CharName> <level>)");
    return false;
  }
  try {
    unsigned long raw = std::stoul(args[0]);
    if (raw > 255u) {
      session->SendNotification("Level out of range.");
      return false;
    }
    auto lv = static_cast<uint8_t>(raw);
    if (lv < 1u)
      lv = 1;
    if (!session->GmSetLevel(lv)) {
      session->SendNotification("Set level failed.");
      return false;
    }
    return true;
  } catch (const std::exception &) {
    session->SendNotification("Invalid level.");
    return false;
  }
}

bool CommandService::HandleBan(std::shared_ptr<ICommandSession> session,
                               const std::vector<std::string> &args,
                               PrivilegeOrigin origin) {
  (void)origin;
  if (!_accountRepo) {
    session->SendNotification("Account repository is not configured.");
    return true;
  }
  if (args.empty()) {
    session->SendNotification("Usage: .ban <username>  (world console only)");
    return false;
  }
  std::string const userUpper = Crypto::ToUpper(args[0]);
  if (!_accountRepo->FindByUsername(userUpper)) {
    session->SendNotification("Unknown account: " + userUpper);
    return true;
  }
  _accountRepo->SetLockedByUsername(userUpper, true);
  session->SendNotification("Banned (login locked): " + userUpper);
  return true;
}

bool CommandService::HandleUnban(std::shared_ptr<ICommandSession> session,
                                  const std::vector<std::string> &args,
                                  PrivilegeOrigin origin) {
  (void)origin;
  if (!_accountRepo) {
    session->SendNotification("Account repository is not configured.");
    return true;
  }
  if (args.empty()) {
    session->SendNotification("Usage: .unban <username>  (world console only)");
    return false;
  }
  std::string const userUpper = Crypto::ToUpper(args[0]);
  if (!_accountRepo->FindByUsername(userUpper)) {
    session->SendNotification("Unknown account: " + userUpper);
    return true;
  }
  _accountRepo->SetLockedByUsername(userUpper, false);
  session->SendNotification("Unbanned: " + userUpper);
  return true;
}

bool CommandService::HandleTicket(std::shared_ptr<ICommandSession> session,
                                  const std::vector<std::string> &args,
                                  PrivilegeOrigin origin) {
  (void)origin;
  if (!_gmTicketService || !_characterService || !_onlineCharacters) {
    session->SendNotification("Ticket system is not configured.");
    return true;
  }
  if (args.empty()) {
    session->SendNotification(
        "Usage: .ticket queue | .ticket mine | .ticket take <id> | .ticket "
        "reply <id> <message> | .ticket close <id>");
    return true;
  }

  std::string const sub = args[0];
  if (sub == "queue") {
    auto list = _gmTicketService->ListQueue(20);
    if (list.empty()) {
      session->SendNotification("Ticket queue is empty.");
      return true;
    }
    for (auto const &t : list) {
      session->SendNotification("Ticket #" + std::to_string(t.id) + " char=" +
                                std::to_string(t.characterGuid));
    }
    return true;
  }
  if (sub == "mine") {
    auto list = _gmTicketService->ListAssignedTo(session->GetAccountId(), 20);
    if (list.empty()) {
      session->SendNotification("No tickets assigned to you.");
      return true;
    }
    for (auto const &t : list) {
      session->SendNotification("Ticket #" + std::to_string(t.id) + " char=" +
                                std::to_string(t.characterGuid));
    }
    return true;
  }
  if (sub == "take") {
    if (args.size() < 2) {
      session->SendNotification("Usage: .ticket take <id>");
      return true;
    }
    uint64_t id = 0;
    try {
      id = std::stoull(args[1]);
    } catch (...) {
      session->SendNotification("Invalid ticket id.");
      return true;
    }
    if (_gmTicketService->AssignToStaff(id, session->GetAccountId()))
      session->SendNotification("Ticket assigned to you.");
    else
      session->SendNotification("Could not take ticket (already assigned or closed).");
    return true;
  }
  if (sub == "reply") {
    if (args.size() < 3) {
      session->SendNotification("Usage: .ticket reply <id> <message>");
      return true;
    }
    uint64_t id = 0;
    try {
      id = std::stoull(args[1]);
    } catch (...) {
      session->SendNotification("Invalid ticket id.");
      return true;
    }
    std::string const text = JoinArgs(args.begin() + 2, args.end());
    if (!_gmTicketService->StaffReply(id, session->GetAccountId(), text)) {
      session->SendNotification("Reply failed (not your ticket or bad state).");
      return true;
    }
    auto updated = _gmTicketService->GetById(id);
    if (!updated) {
      session->SendNotification("Reply saved.");
      return true;
    }
    auto ch = _characterService->GetCharacterByGuid(updated->characterGuid);
    if (!ch) {
      session->SendNotification("Reply saved (character offline).");
      return true;
    }
    if (auto target = _onlineCharacters->TryResolve(ch->GetName())) {
      target->SendGmResponseReceived(static_cast<uint32_t>(updated->id),
                                     updated->message, updated->gmResponse);
    }
    session->SendNotification("Reply sent.");
    return true;
  }
  if (sub == "close") {
    if (args.size() < 2) {
      session->SendNotification("Usage: .ticket close <id>");
      return true;
    }
    uint64_t id = 0;
    try {
      id = std::stoull(args[1]);
    } catch (...) {
      session->SendNotification("Invalid ticket id.");
      return true;
    }
    if (_gmTicketService->StaffClose(id, session->GetAccountId()))
      session->SendNotification("Ticket closed.");
    else
      session->SendNotification("Close failed (not your ticket or already closed).");
    return true;
  }

  session->SendNotification("Unknown .ticket subcommand.");
  return true;
}

void CommandService::SetShutdownRequestHandler(std::function<void()> handler) {
  _shutdownRequestHandler = std::move(handler);
}

bool CommandService::HandleEmail(std::shared_ptr<ICommandSession> session,
                                 const std::vector<std::string> &args,
                                 PrivilegeOrigin origin) {
  (void)args;
  if (origin != PrivilegeOrigin::GameClient) {
    session->SendNotification(".email is only available in-game.");
    return true;
  }
  if (session->GetActiveCharacterObjectGuid() == 0) {
    session->SendNotification("You must be in world to use .email.");
    return true;
  }
  session->OpenGmMailboxUi();
  return true;
}

void CommandService::PollScheduledRestart() {
  if (!_restartDeadline)
    return;
  auto const now = std::chrono::steady_clock::now();
  if (now >= *_restartDeadline) {
    _restartDeadline.reset();
    _restartAnnouncedDownTo = 0;
    LOG_INFO("Scheduled server restart: timer elapsed; stopping world loop.");
    if (_shutdownRequestHandler)
      _shutdownRequestHandler();
    else
      LOG_CRITICAL("Scheduled restart elapsed but no shutdown handler is set.");
    return;
  }
  if (!_onlineCharacters)
    return;
  int const rem = CeilSecondsRemaining(now, *_restartDeadline);
  if (rem < _restartAnnouncedDownTo && rem >= 1 && rem <= 10) {
    std::string const plain =
        "Restarting in " + std::to_string(rem) + (rem == 1 ? " second." : " seconds.");
    _onlineCharacters->BroadcastAnnouncement("|cffFFCC00[Server]|r " + plain, plain);
    _restartAnnouncedDownTo = rem;
  }
}

bool CommandService::HandleServer(std::shared_ptr<ICommandSession> session,
                                    const std::vector<std::string> &args,
                                    PrivilegeOrigin origin) {
  (void)origin;
  if (args.empty() || !AsciiEqualsLower(args[0], "restart")) {
    session->SendNotification(
        "Usage: .server restart <delay>   (delay: number + s or m, e.g. 30s, 5m)");
    return false;
  }
  if (args.size() < 2) {
    session->SendNotification("Usage: .server restart <delay>   (examples: 30s, 5m)");
    return false;
  }
  std::chrono::seconds delay{0};
  if (!ParseRestartDelayToken(args[1], delay)) {
    session->SendNotification(
        "Invalid delay. Use a positive value with unit s (seconds) or m (minutes), "
        "e.g. 30s or 5m (max 7 days).");
    return false;
  }
  auto const deadline =
      std::chrono::steady_clock::now() +
      std::chrono::duration_cast<std::chrono::steady_clock::duration>(delay);
  _restartDeadline = deadline;
  _restartAnnouncedDownTo = static_cast<int>(delay.count());

  std::string const lead = FormatRestartLeadIn(delay);
  std::string const plain = "The server will restart in " + lead;
  if (_onlineCharacters)
    _onlineCharacters->BroadcastAnnouncement("|cffFFCC00[Server]|r " + plain, plain);
  session->SendNotification("Scheduled: " + plain);
  LOG_DEBUG("[server] {}", plain);
  return true;
}

} // namespace Firelands
