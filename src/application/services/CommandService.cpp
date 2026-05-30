#include "CommandService.h"
#include <application/ports/ICommandSession.h>
#include <application/services/GmTicketService.h>
#include <application/services/OnlineCharacterSessionRegistry.h>
#include <application/services/CharacterService.h>
#include <application/services/WorldService.h>
#include <domain/models/Character.h>
#include <domain/world/Creature.h>
#include <domain/world/Map.h>
#include <application/services/SRPService.h>
#include <domain/repositories/IAccountRepository.h>
#include <shared/Common.h>
#include <shared/Crypto.h>
#include <shared/game/AccessLevel.h>
#include <shared/game/Permissions.h>
#include <shared/Logger.h>
#include <shared/network/MovementInfo.h>
#include <shared/network/UpdateData.h>
#include <shared/network/WorldPacket.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <cmath>
#include <cctype>
#include <chrono>
#include <cstring>
#include <iterator>
#include <limits>
#include <sstream>
#include <string_view>

namespace Firelands {

namespace {

/// Blood Elf Female civilian — usable placeholder when `.npc add` omits displayId.
constexpr uint32_t kDefaultGmNpcDisplayId = 15688u;

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

  void SendPacket(WorldPacket &packet) override {
    if (_operatorSession)
      _operatorSession->SendPacket(packet);
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

  void OpenGmTicketUi() override { _subject->OpenGmTicketUi(); }

  uint32 GetMapId() const override { return _subject->GetMapId(); }

  void RequestDisconnect(std::string const &reason) override {
    _subject->RequestDisconnect(reason);
  }

  bool GmLearnSpell(uint32 spellId) override {
    return _subject->GmLearnSpell(spellId);
  }

  bool GmUnlearnSpell(uint32 spellId) override {
    return _subject->GmUnlearnSpell(spellId);
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

  bool GmResetAllCooldowns() override { return _subject->GmResetAllCooldowns(); }

  bool GmDamageUnit(uint64 targetGuid, uint32 amount) override {
    return _subject->GmDamageUnit(targetGuid, amount);
  }

  bool GmSpawnNpc(uint32 creatureEntry, uint32 displayId,
                  uint32 factionTemplateOrZeroDefault) override {
    return _subject->GmSpawnNpc(creatureEntry, displayId,
                               factionTemplateOrZeroDefault);
  }

  bool GmDeleteNpcByObjectGuid(uint64 objectGuid) override {
    return _subject->GmDeleteNpcByObjectGuid(objectGuid);
  }

  bool GmSetForcedFactionReaction(uint32 factionDbcId,
                                  uint8 reputationRank) override {
    return _subject->GmSetForcedFactionReaction(factionDbcId, reputationRank);
  }

  bool GmClearForcedFactionReaction(uint32 factionDbcId) override {
    return _subject->GmClearForcedFactionReaction(factionDbcId);
  }

  bool GmClearAllForcedFactionReactions() override {
    return _subject->GmClearAllForcedFactionReactions();
  }

  bool GmSetOwnFactionTemplate(uint32 factionTemplate) override {
    return _subject->GmSetOwnFactionTemplate(factionTemplate);
  }

  bool GmSetSelectedCreatureFactionTemplate(uint32 factionTemplate) override {
    return _subject->GmSetSelectedCreatureFactionTemplate(factionTemplate);
  }

  bool GmNpcSearchPrintResults(std::string const &nameQuery) override {
    return _subject->GmNpcSearchPrintResults(nameQuery);
  }

  bool GmNpcPrintTargetInfo() override { return _subject->GmNpcPrintTargetInfo(); }

  bool GmRevivePlayer(uint64 playerGuid) override {
    return _subject->GmRevivePlayer(playerGuid);
  }

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

static char const *FindPathStatusName(FindPathStatus status) {
  switch (status) {
  case FindPathStatus::Complete:
    return "Complete";
  case FindPathStatus::Partial:
    return "Partial";
  case FindPathStatus::NoPath:
    return "NoPath";
  case FindPathStatus::NavMeshMissing:
    return "NavMeshMissing";
  }
  return "Unknown";
}

static std::string FormatVec3(Vec3 const &v) {
  std::ostringstream ss;
  ss << "(" << v.x << ", " << v.y << ", " << v.z << ")";
  return ss.str();
}

static void SendMmapMarkerCreate(std::shared_ptr<ICommandSession> const &session,
                                 uint32_t mapId, uint64_t markerGuid,
                                 Vec3 const &pos, uint32_t entry,
                                 uint32_t displayId,
                                 uint32_t factionTemplate) {
  MovementInfo move{};
  move.x = pos.x;
  move.y = pos.y;
  move.z = pos.z;
  move.orientation = 0.0f;

  UpdateData update(static_cast<uint16>(mapId));
  update.AddCreateObject(
      markerGuid, TYPEID_UNIT, move,
      WorldSessionObjectUpdate::BuildMinimalNpcUnitCreateFields(
          markerGuid, entry, displayId, 1u, 1u, 1u, 0u, factionTemplate));
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  session->SendPacket(pkt);
}

static void SendMmapMarkerDespawn(std::shared_ptr<ICommandSession> const &session,
                                  uint32_t mapId, uint64_t markerGuid) {
  UpdateData update(static_cast<uint16>(mapId));
  update.AddOutOfRangeObjects({markerGuid});
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  session->SendPacket(pkt);
}

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
  RegisterCommand("mmap", {[this](auto s, auto a, auto o) { return HandleMmap(s, a, o); },
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
      "unlearn", {[this](auto s, auto a, auto o) { return HandleUnlearn(s, a, o); },
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
      "cd", {[this](auto s, auto a, auto o) { return HandleCd(s, a, o); },
             ToMask(Permission::CommandGameplay), CommandAvailability::Both,
             ConsoleArgLayout::TargetOnlineCharacterFirst});
  RegisterCommand(
      "damage", {[this](auto s, auto a, auto o) { return HandleDamage(s, a, o); },
                 ToMask(Permission::CommandGameplay), CommandAvailability::Game,
                 ConsoleArgLayout::SameAsInGame});
  RegisterCommand(
      "revive", {[this](auto s, auto a, auto o) { return HandleRevive(s, a, o); },
                 ToMask(Permission::CommandGameplay), CommandAvailability::Game,
                 ConsoleArgLayout::SameAsInGame});
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
  RegisterCommand(
      "npc", {[this](auto s, auto a, auto o) { return HandleNpc(s, a, o); },
             ToMask(Permission::ServerControl), CommandAvailability::Both,
             ConsoleArgLayout::TargetOnlineCharacterFirst});
  RegisterCommand(
      "faction",
      {[this](auto s, auto a, auto o) { return HandleFaction(s, a, o); },
       ToMask(Permission::CommandGameplay), CommandAvailability::Both,
       ConsoleArgLayout::TargetOnlineCharacterFirst});
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
    bool const moderatorHelp =
        HasAtLeast(acc, AccessLevel::Moderator) &&
        (AsciiEqualsLower(firstToken, "help") ||
         AsciiEqualsLower(firstToken, "commands"));
    if (!HasAtLeast(acc, AccessLevel::GameMaster) && !moderatorEmail &&
        !moderatorHelp) {
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

bool CommandService::HandleMmap(std::shared_ptr<ICommandSession> session,
                                const std::vector<std::string> &args,
                                PrivilegeOrigin origin) {
  (void)origin;
  auto collision = WorldService::Instance().GetCollisionQueries();
  if (!collision) {
    LOG_ERROR("MMAP: collision service is not configured.");
    session->SendNotification("MMAP: collision service is not configured.");
    return true;
  }

  MovementInfo const &playerPos = session->GetPosition();
  uint32_t mapId = session->GetMapId();
  uint64_t const playerGuid = session->GetActiveCharacterObjectGuid();

  LOG_DEBUG("MMAP request: playerGuid={} mapId={} args={}", playerGuid, mapId,
            args.empty() ? std::string("<target>") : JoinArgs(args.begin(), args.end()));

  // Auto-remove expired markers (older than 9s)
  {
    auto mit = _mmapMarkers.find(playerGuid);
    if (mit != _mmapMarkers.end()) {
      auto const now = std::chrono::steady_clock::now();
      auto &markers = mit->second;
      markers.erase(
          std::remove_if(markers.begin(), markers.end(),
                         [&](auto const &p) {
                           if (now - p.second > std::chrono::seconds(9)) {
                             SendMmapMarkerDespawn(session, mapId, p.first);
                             return true;
                           }
                           return false;
                         }),
          markers.end());
      if (markers.empty())
        _mmapMarkers.erase(mit);
    }
  }

  // .mmap clear — remove visual markers
  if (!args.empty() && args[0] == "clear") {
    LOG_DEBUG("MMAP clear: playerGuid={} mapId={}", playerGuid, mapId);
    ClearMmapMarkers(session, playerGuid, mapId);
    session->SendNotification("MMAP: visual markers removed.");
    return true;
  }

  // Determine start/end positions for pathfinding
  Vec3 startPos{playerPos.x, playerPos.y, playerPos.z};
  Vec3 endPos{playerPos.x, playerPos.y, playerPos.z};
  bool checkPath = false;
  std::string pathLabel;

  try {
    if (!args.empty()) {
      if (args.size() < 3) {
        LOG_WARN("MMAP invalid coordinates: playerGuid={} mapId={} argCount={}",
                 playerGuid, mapId, args.size());
        session->SendNotification("Usage: .mmap [x y z [mapId]]  |  .mmap (with target)  |  .mmap clear");
        return false;
      }
      endPos.x = std::stof(args[0]);
      endPos.y = std::stof(args[1]);
      endPos.z = std::stof(args[2]);
      if (args.size() > 3)
        mapId = static_cast<uint32_t>(std::stoul(args[3]));
      pathLabel = "you -> " + FormatVec3(endPos);
      checkPath = true;
    } else {
      // Check if a creature is targeted: path from creature to player
      uint64_t const targetGuid = session->GetClientSelectionGuid();
      if (targetGuid != 0) {
        auto map = WorldService::Instance().GetMap(mapId);
        if (map) {
          auto creature = map->TryGetCreature(targetGuid);
          if (creature) {
            MovementInfo const &crPos = creature->GetPosition();
            startPos = Vec3{crPos.x, crPos.y, crPos.z};
            endPos = Vec3{playerPos.x, playerPos.y, playerPos.z};
            pathLabel = std::string("creature(") + std::to_string(creature->GetEntry()) +
                        ") -> you";
            checkPath = true;
            LOG_DEBUG("MMAP target creature resolved: playerGuid={} targetGuid={} entry={} mapId={}",
                      playerGuid, targetGuid, creature->GetEntry(), mapId);
          }
        }
        if (!checkPath) {
          LOG_WARN("MMAP target is not a creature or not on current map: playerGuid={} targetGuid={} mapId={}",
                   playerGuid, targetGuid, mapId);
          session->SendNotification("MMAP: targeted object is not a creature.");
        }
      }
    }
  } catch (std::exception const &) {
    LOG_ERROR("MMAP invalid coordinates parse error: playerGuid={} mapId={} args={}",
              playerGuid, mapId,
              args.empty() ? std::string("<target>") : JoinArgs(args.begin(), args.end()));
    session->SendNotification("MMAP: invalid coordinates.");
    return false;
  }

  bool const available = collision->IsNavMeshDataAvailable(mapId);
  LOG_DEBUG("MMAP navmesh availability: mapId={} available={}", mapId, available);
  float const height = collision->GetHeight(mapId, playerPos.x, playerPos.y, playerPos.z);
  std::ostringstream head;
  head << "MMAP map=" << mapId << " navmesh=" << (available ? "loaded" : "missing")
       << " player=" << FormatVec3(Vec3{playerPos.x, playerPos.y, playerPos.z})
       << " height=" << height;
  session->SendNotification(head.str());

  if (!checkPath)
    return true;

  FindPathRequest req;
  req.mapId = mapId;
  req.startX = startPos.x;
  req.startY = startPos.y;
  req.startZ = startPos.z;
  req.endX = endPos.x;
  req.endY = endPos.y;
  req.endZ = endPos.z;
  req.smoothPath = true;
  req.allowPartialPath = true;

  FindPathResult result = collision->FindPath(req);
  LOG_DEBUG("MMAP path result: mapId={} status={} waypoints={}", mapId,
            FindPathStatusName(result.status), result.waypoints.size());
  std::ostringstream path;
  path << "MMAP path " << FormatVec3(startPos) << " -> "
       << FormatVec3(endPos) << "  (" << pathLabel << ")"
       << " status=" << FindPathStatusName(result.status)
       << " waypoints=" << result.waypoints.size();
  session->SendNotification(path.str());

  constexpr size_t kMaxPrintedWaypoints = 8;
  for (size_t i = 0; i < result.waypoints.size() && i < kMaxPrintedWaypoints; ++i) {
    session->SendNotification("  wp[" + std::to_string(i) + "] " +
                              FormatVec3(result.waypoints[i]));
  }
  if (result.waypoints.size() > kMaxPrintedWaypoints) {
    session->SendNotification("  ... " +
                              std::to_string(result.waypoints.size() -
                                             kMaxPrintedWaypoints) +
                              " more waypoint(s)");
  }

  // Spawn visual markers at each waypoint (auto-despawn after 9s)
  ClearMmapMarkers(session, playerGuid, mapId);

  if (!result.waypoints.empty()) {
    constexpr uint32_t kMarkerDisplayId = 11686u;
    constexpr uint32_t kMarkerEntry = 99999u;
    uint64_t baseGuid = static_cast<uint64_t>(0xF100000000000000ull) +
                        (static_cast<uint64_t>(playerGuid & 0xFFFFu) << 32);
    auto const now = std::chrono::steady_clock::now();
    std::vector<std::pair<uint64_t, std::chrono::steady_clock::time_point>> markers;
    markers.reserve(result.waypoints.size());

    for (size_t i = 0; i < result.waypoints.size(); ++i) {
      auto const &wp = result.waypoints[i];
      float const z = wp.z + 1.0f;
      uint64_t const markerGuid = baseGuid + i;
      SendMmapMarkerCreate(session, mapId, markerGuid,
                           Vec3{wp.x, wp.y, z}, kMarkerEntry,
                           kMarkerDisplayId, Creature::kDefaultFactionTemplate);
      LOG_TRACE("MMAP marker spawn: mapId={} guid={} wpIndex={} pos=({}, {}, {})",
                mapId, markerGuid, i, wp.x, wp.y, z);
      markers.emplace_back(markerGuid, now);
    }
    _mmapMarkers[playerGuid] = std::move(markers);
    session->SendNotification("MMAP: " +
                              std::to_string(result.waypoints.size()) +
                              " marker(s) spawned (despawn in 9s). |cffffffff.mmap clear|r to remove earlier.");
  }
  return true;
}

void CommandService::ClearMmapMarkers(std::shared_ptr<ICommandSession> session,
                                     uint64_t playerGuid, uint32_t mapId) {
  auto it = _mmapMarkers.find(playerGuid);
  if (it == _mmapMarkers.end())
    return;

  // Remove expired + all markers
  auto &markers = it->second;
  for (auto &[guid, spawnTime] : markers) {
    (void)spawnTime;
    SendMmapMarkerDespawn(session, mapId, guid);
  }
  markers.clear();
  _mmapMarkers.erase(it);
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
    uint32 mapId = (args.size() > 3) ? static_cast<uint32>(std::stoul(args[3]))
                                      : session->GetMapId();

    session->TeleportTo(mapId, x, y, z);
    return true;
  } catch (const std::exception &) {
    session->SendNotification("Error: Invalid arguments.");
    return false;
  }
}

namespace {

enum class HelpChunkAudience : uint8_t { Both, Game, Console };

struct StaffHelpChunk {
  HelpChunkAudience audience;
  PermissionMask required;
  char const *wow;
  char const *plain;
};

static void EmitConsoleLinesFromPlain(std::shared_ptr<ICommandSession> session,
                                      char const *plain) {
  std::string_view v{plain};
  while (!v.empty()) {
    size_t const pos = v.find('\n');
    if (pos == std::string_view::npos) {
      session->SendNotification(std::string(v));
      break;
    }
    session->SendNotification(std::string(v.substr(0, pos)));
    v.remove_prefix(pos + 1);
  }
}

static bool HelpChunkVisible(HelpChunkAudience aud, PrivilegeOrigin origin) {
  if (origin == PrivilegeOrigin::GameClient)
    return aud != HelpChunkAudience::Console;
  return aud != HelpChunkAudience::Game;
}

static constexpr StaffHelpChunk kStaffHelpChunks[] = {
    {HelpChunkAudience::Console, 0,
     "|cffFCE566=== Firelands - console commands ===|r\n"
     "|cffAAAAAAEvery command starts with|r |cffffffff.|r\n"
     "|cffAAAAAAOnly commands you may use are listed|r |cff888888(privileges).|r\n"
     "|cffAAAAAAPlayers receive no response from staff dot-commands.|r",
     R"H0(================================================================================
Firelands - console command reference
================================================================================

Every command starts with a dot (.). This list is filtered to your effective
privileges on the server console.

)H0"},
    {HelpChunkAudience::Game, 0,
     "|cffFCE566=== Firelands - staff command help ===|r\n"
     "|cffAAAAAAEvery command starts with|r |cffffffff.|r\n"
     "|cffAAAAAAOnly commands you may use are listed|r |cff888888(privileges and "
     "where the command may run).|r\n"
     "|cffAAAAAAPlayers receive no response from staff dot-commands.|r",
     R"H0G(================================================================================
Firelands - staff command reference
================================================================================

Every command starts with a dot (.). This list omits in-game-only commands and
is filtered to your effective privileges on the server console.

)H0G"},
    {HelpChunkAudience::Both, 0,
     "|cffFFD200· Help|r\n"
     "|cffCCCCCC.help|r |cff888888—|r Show this filtered guide.  |cff666666e.g.|r "
     "|cffffffff.help|r\n"
     "|cffCCCCCC.commands|r |cff888888—|r Same as .help.  |cff666666e.g.|r "
     "|cffffffff.commands|r",
     R"H1(--------------------------------------------------------------------------------
Help
--------------------------------------------------------------------------------

  .help   .commands
      Show this filtered guide.
)H1"},
    {HelpChunkAudience::Game, ToMask(Permission::CommandMailbox),
     "|cffFFD200· Mailbox|r\n"
     "|cffCCCCCC.email|r |cff888888—|r Open mailbox UI without a mailbox NPC.  "
     "|cff666666e.g.|r |cffffffff.email|r",
     R"H2(--------------------------------------------------------------------------------
Mailbox  (Moderator+ in-game)
--------------------------------------------------------------------------------

  .email
      Open mailbox UI without a nearby mailbox NPC.
)H2"},
    {HelpChunkAudience::Both, ToMask(Permission::CommandGps),
     "|cffFFD200· Position|r\n"
     "|cffCCCCCC.gps|r |cff888888—|r Print X, Y, Z, facing.  |cff666666e.g.|r "
     "|cffffffff.gps|r\n"
     "|cff666666Console (online character first):|r |cffffffff.gps Annabell|r\n"
     "|cffCCCCCC.mmap|r |cff888888—|r Check navmesh and path waypoints.  "
     "|cff666666e.g.|r |cffffffff.mmap -8759 544 97|r",
     R"H3(--------------------------------------------------------------------------------
Position
--------------------------------------------------------------------------------

  .gps   [.gps <OnlineCharName>]
      Print X, Y, Z, and facing. From console, prefix an online character name.

  .mmap [x y z [mapId]]
  .mmap <OnlineCharName> [x y z [mapId]]   (console)
      Check whether navmesh data is loaded for the current map. With coordinates,
      calculate a path and print the status plus the first waypoints.
)H3"},
    {HelpChunkAudience::Both, ToMask(Permission::CommandTeleport),
     "|cffFFD200· Teleport|r\n"
     "|cffCCCCCC.tele|r |cff888888—|r Teleport to coordinates.  "
     "|cff666666In-game:|r |cffffffff.tele -8759 544 97|r |cff666666(map optional)|r\n"
     "|cff666666With map id:|r |cffffffff.tele 100 -50 25 571|r\n"
     "|cff666666Console:|r |cffffffff.tele Annabell -8759 544 97|r  |cff666666or|r  "
     "|cffffffff.tele Annabell -8759 544 97 0|r",
     R"H4(--------------------------------------------------------------------------------
Teleport
--------------------------------------------------------------------------------

  .tele <x> <y> <z> [mapId]   (in-game; current map if map id omitted)
  .tele <CharName> <x> <y> <z> [mapId]   (console; online character first)
)H4"},
    {HelpChunkAudience::Both, ToMask(Permission::ManagePlayers),
     "|cffFFD200· Online players|r\n"
     "|cffCCCCCC.online|r |cff888888/|r |cffCCCCCC.who|r |cff888888—|r Online totals "
     "by faction.  |cff666666e.g.|r |cffffffff.online|r\n"
     "|cffCCCCCC.announce|r |cff888888—|r Server-wide system message.  "
     "|cff666666e.g.|r |cffffffff.announce Welcome|r\n"
     "|cffCCCCCC.kick|r |cff888888—|r Disconnect an online character.  "
     "|cff666666e.g.|r |cffffffff.kick BadActor exploiting|r\n"
     "|cffCCCCCC.goto|r |cff888888/|r |cffCCCCCC.appear|r |cff888888—|r Teleport to "
     "player.  |cff666666Console:|r |cffffffff.goto Staff Target|r\n"
     "|cffCCCCCC.summon|r |cff888888—|r Bring player to anchor.  "
     "|cff666666Console:|r |cffffffff.summon Victim Anchor|r",
     R"H5(--------------------------------------------------------------------------------
Online players
--------------------------------------------------------------------------------

  .online   .who
  .announce <message>
  .kick <CharName> [reason]
  .goto / .appear   (console: .goto <StaffChar> <TargetChar>)
  .summon   (console: .summon <VictimChar> <AnchorChar>)
)H5"},
    {HelpChunkAudience::Game, ToMask(Permission::ManageGmTickets),
     "|cffFFD200· GM tickets|r |cff666666(in-game only)|r\n"
     "|cffCCCCCC.ticket queue|r |cff888888—|r Unassigned queue.  "
     "|cff666666e.g.|r |cffffffff.ticket queue|r\n"
     "|cffCCCCCC.ticket mine|r |cff888888—|r Assigned to you.  "
     "|cff666666e.g.|r |cffffffff.ticket mine|r\n"
     "|cffCCCCCC.ticket take|r |cff888888—|r |cff666666e.g.|r |cffffffff.ticket take "
     "1|r\n"
     "|cffCCCCCC.ticket reply|r |cff888888—|r |cff666666e.g.|r "
     "|cffffffff.ticket reply 1 Thanks.|r\n"
     "|cffCCCCCC.ticket close|r |cff888888—|r |cff666666e.g.|r "
     "|cffffffff.ticket close 1|r\n"
     "|cffCCCCCC.ticket ui|r |cff888888—|r Open ticket desk (gossip window).  "
     "|cff666666e.g.|r |cffffffff.ticket ui|r",
     nullptr},
    {HelpChunkAudience::Both, ToMask(Permission::CommandGameplay),
     "|cffFFD200· Gameplay (GM)|r\n"
     "|cffCCCCCC.learn|r |cff888888—|r Learn spell by id.  "
     "|cff666666Console:|r |cffffffff.learn CharName 475|r\n"
     "|cffCCCCCC.unlearn|r |cff888888—|r Remove spell by id.  "
     "|cff666666Console:|r |cffffffff.unlearn CharName 475|r\n"
     "|cffCCCCCC.money|r |cff888888—|r Add/remove copper (signed).  "
     "|cff666666Console:|r |cffffffff.money CharName 50000|r\n"
     "|cffCCCCCC.level|r |cff888888—|r Set level 1-85.  "
     "|cff666666Console:|r |cffffffff.level Char 60|r\n"
     "|cffCCCCCC.cd|r |cff888888—|r Reset all spell/GCD/racial cooldowns.  "
     "|cff666666Console:|r |cffffffff.cd CharName|r\n"
     "|cffCCCCCC.damage|r |cff888888—|r Damage selected unit "
     "|cff666666(in-game only)|r  |cff666666e.g.|r |cffffffff.damage 500|r\n"
     "|cffCCCCCC.revive|r |cff888888—|r Full health/power; revives targeted "
     "player or self "
     "|cff666666(in-game)|r  |cff666666e.g.|r |cffffffff.revive|r\n"
     "|cffAAAAAAItems:|r |cffCCCCCC.additem|r |cff888888/|r |cffCCCCCC.delitem|r  "
     "|cffAAAAAAsee next block.|r",
     R"H6(--------------------------------------------------------------------------------
Gameplay  (GM)
--------------------------------------------------------------------------------

  .learn <spellId>              (console: .learn <CharName> <spellId>)
  .unlearn <spellId>            (console: .unlearn <CharName> <spellId>)
  .money <copper delta>         (console: .money <CharName> <copper>)
  .level <1-85>                 (console: .level <CharName> <level>)
  .cd                          (console: .cd <CharName>)
  .damage <amount>              (in-game only; target a unit first)
  .revive                      (in-game; target player or self to full health/power)
  .additem / .delitem           (see Items below)
)H6"},
    {HelpChunkAudience::Both, ToMask(Permission::CommandGameplay),
     "|cffFFD200· Items (GM)|r\n"
     "|cff666666In-game:|r numeric first arg with target selected sends item to "
     "target (|cffffffff.additem 6948 1|r). Name first: "
     "|cffffffff.additem Annabell 6948 5|r\n"
     "|cff666666Console:|r name first: |cffffffff.additem Annabell 6948 1|r\n"
     "|cffAAAAAAFull main backpack|r |cff666666(slots 23-38)|r |cffAAAAAAstores to|r "
     "|cffCCCCCCmail|r|cffAAAAAA; use|r |cffCCCCCC.email|r |cffAAAAAAin-game.|r\n"
     "|cffCCCCCC.delitem|r |cff888888—|r Same targeting; main backpack only.",
     R"H7(--------------------------------------------------------------------------------
Items  (GM)
--------------------------------------------------------------------------------

  In-game: first arg all digits -> item entry to selected target (.additem 6948 1).
      Otherwise first token is an online character name (.additem Annabell 6948 5).

  Console: character name first: .additem Annabell 6948 1

  Full main backpack (slots 23-38) -> DB mail; use .email in-game to open mailbox.

  .delitem   same targeting; main backpack only (not equipped).
)H7"},
    {HelpChunkAudience::Game, ToMask(Permission::CommandGmTools),
     "|cffFFD200· Tags & movement|r\n"
     "|cffCCCCCC.gm on|r |cff888888/|r |cffCCCCCC.gm off|r  "
     "|cffCCCCCC.dnd on|r|cff888888/|r|cffCCCCCC.dnd off|r  "
     "|cffCCCCCC.dev on|r|cff888888/|r|cffCCCCCC.dev off|r\n"
     "|cffCCCCCC.visible on|r |cff888888/|r |cffCCCCCC.visible off|r  "
     "|cffCCCCCC.fly on|r|cff888888/|r|cffCCCCCC.fly off|r  "
     "|cffCCCCCC.speed|r |cff888888—|r run/flight speed or |cffffffff.speed reset|r",
     R"H8(--------------------------------------------------------------------------------
Tags & movement
--------------------------------------------------------------------------------

  .gm on | off    .dnd on | off    .dev on | off    .visible on | off
  .fly on | off   .speed <n> | .speed reset
)H8"},
    {HelpChunkAudience::Console, ToMask(Permission::ServerControl),
     "|cffFFD200· NPC & server|r |cff666666(console)|r\n"
     "|cffCCCCCC.npc search|r |cff888888—|r |cff666666e.g.|r "
     "|cffffffff.npc Char search wolf|r\n"
     "|cffCCCCCC.npc add|r |cff888888—|r |cff666666e.g.|r |cffffffff.npc add 2575 "
     "[displayId] [factionTemplate]|r\n"
     "|cffCCCCCC.npc del|r |cff888888—|r |cff666666e.g.|r "
     "|cffffffff.npc CharName del <guid>|r\n"
     "|cffCCCCCC.server restart|r |cff888888—|r Delay |cffffffff30s|r / "
     "|cffffffff5m|r|cff666666; last 10s countdown lines.|r",
     R"H9(--------------------------------------------------------------------------------
NPC & server  (console)
--------------------------------------------------------------------------------

  .npc search <fragment>   (console: .npc <CharName> search <fragment>)
  .npc add <entry> [displayId] [factionTemplate]
  .npc del   (console: .npc <CharName> del <guid>)

  .server restart <delay>   e.g. 30s, 5m
)H9"},
    {HelpChunkAudience::Game, ToMask(Permission::ServerControl),
     "|cffFFD200· NPC & faction|r |cff666666(in-game)|r\n"
     "|cffCCCCCC.npc info|r |cff888888—|r Colored gossip inspector on target NPC "
     "|cff666666e.g.|r |cffffffff.npc info|r\n"
     "|cffCCCCCC.npc add|r |cff888888—|r |cff666666e.g.|r |cffffffff.npc add 2575 "
     "[displayId] [factionTemplate]|r\n"
     "|cffCCCCCC.faction forced set|r |cff888888—|r "
     "|cffffffff.faction forced set <id> <0-7>|r\n"
     "|cffCCCCCC.faction template|r |cff888888—|r "
     "|cffffffff.faction template self|target <template>|r",
     R"H9G(--------------------------------------------------------------------------------
NPC & faction  (in-game)
--------------------------------------------------------------------------------

  .npc info                    (target NPC; opens GM gossip inspector)
  .npc add <entry> [displayId] [factionTemplate]
  .faction forced set <factionDbcId> <0-7> | forced clear <id> | forced clearall
  .faction template self|target <factionTemplate>
)H9G"},
    {HelpChunkAudience::Console, ToMask(Permission::ManageAccounts),
     "|cffFFD200· Auth DB|r |cff666666(console only)|r\n"
     "|cffCCCCCC.account create|r ... |cffCCCCCC.account delete|r ... "
     "|cffCCCCCC.account setaccess|r ... |cffCCCCCC.ban|r |cff888888/|r "
     "|cffCCCCCC.unban|r",
     R"HA(--------------------------------------------------------------------------------
Auth database  (console only)
--------------------------------------------------------------------------------

  .account create <user> <pass> [email] [expansion 0-3] [access 0-3]
  .account delete <username>
  .account setaccess <username> <0-3>
  .ban <account>   .unban <account>
)HA"},
    {HelpChunkAudience::Console, 0,
     "|cffAAAAAAShutdown:|r |cffffffffquit|r |cff888888or|r |cffffffffexit|r "
     "|cffAAAAAA(no dot)|r  |cff666666or|r |cffffffff.quit|r |cff666666/|r "
     "|cffffffff.exit|r",
     R"HB(--------------------------------------------------------------------------------
Shut down this world process
--------------------------------------------------------------------------------

  quit   exit   (no leading dot)   or   .quit   .exit
)HB"},
};

static void EmitFilteredStaffHelp(std::shared_ptr<ICommandSession> session,
                                   PrivilegeOrigin origin) {
  AccessLevel const acc = session->GetAccountAccessLevel();
  for (StaffHelpChunk const &ch : kStaffHelpChunks) {
    if (!HelpChunkVisible(ch.audience, origin))
      continue;
    if (ch.required != 0 &&
        !HasPermission(acc, origin, ch.required))
      continue;
    if (origin == PrivilegeOrigin::GameClient) {
      if (ch.wow)
        session->SendNotification(std::string(ch.wow));
    } else if (ch.plain) {
      EmitConsoleLinesFromPlain(session, ch.plain);
    }
  }
}

} // namespace

bool CommandService::HandleHelp(std::shared_ptr<ICommandSession> session,
                                const std::vector<std::string> &,
                                PrivilegeOrigin origin) {
  EmitFilteredStaffHelp(std::move(session), origin);
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
    session->SendNotification(
        "GM tag enabled. All nearby creatures are visible and selectable.");
    return true;
  }
  if (AsciiEqualsLower(args[0], "off")) {
    session->SetGmTagEnabled(false);
    session->SendNotification(
        "GM tag disabled. Creature visibility follows phases and unit flags again.");
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
    session->SendNotification("Flight enabled.");
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
  if (!_onlineCharacters) {
    session->SendNotification("Online registry is not configured.");
    return true;
  }
  OnlineFactionCounts const counts = _onlineCharacters->CountOnlineByFactionSide();
  size_t const n = counts.alliance + counts.horde + counts.unknown;

  auto sendLine = [&](std::string msg) {
    if (origin == PrivilegeOrigin::ServerConsole)
      msg = StripWowChatColorTokens(msg);
    session->SendNotification(std::move(msg));
  };

  std::string const rule =
      "|cff555555------------------------------------------------|r\n";

  if (n == 0) {
    sendLine("|cffFFD200· Online players|r\n" + rule +
             "|cffAAAAAATotal connected|r |cff888888·|r |cffffcc000|r\n"
             "|cff666666No characters are logged in right now.|r");
    return true;
  }

  std::string msg;
  msg.reserve(384);
  msg += "|cffFFD200· Online players|r\n";
  msg += rule;
  msg += "|cffAAAAAATotal connected|r |cff888888·|r |cffffcc00";
  msg += std::to_string(n);
  msg += "|r\n";
  msg += "|cff3399FFAlliance|r |cff888888·|r |cffffcc00";
  msg += std::to_string(counts.alliance);
  msg += "|r\n";
  msg += "|cffCC3333Horde|r |cff888888·|r |cffffcc00";
  msg += std::to_string(counts.horde);
  msg += "|r";
  if (counts.unknown != 0) {
    msg += "\n|cffAAAAAAUnknown faction|r |cff888888·|r |cffffcc00";
    msg += std::to_string(counts.unknown);
    msg += "|r";
  }

  sendLine(std::move(msg));
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

bool CommandService::HandleUnlearn(std::shared_ptr<ICommandSession> session,
                                   const std::vector<std::string> &args,
                                   PrivilegeOrigin origin) {
  (void)origin;
  if (args.empty()) {
    session->SendNotification(
        "Usage: .unlearn <spellId>  (world console: .unlearn <CharName> <spellId>)");
    return false;
  }
  try {
    uint32 const sid = static_cast<uint32>(std::stoul(args[0]));
    if (!session->GmUnlearnSpell(sid)) {
      session->SendNotification("Unlearn failed.");
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

bool CommandService::HandleRevive(std::shared_ptr<ICommandSession> session,
                                  const std::vector<std::string> &args,
                                  PrivilegeOrigin origin) {
  (void)args;
  (void)origin;
  uint64_t const targetGuid = session->GetClientSelectionGuid();
  if (targetGuid != 0) {
    if (session->GmRevivePlayer(targetGuid))
      return true;
    session->SendNotification(
        "Revive failed: select a player, or clear selection to revive yourself.");
    return false;
  }
  if (!session->GmReviveSelf()) {
    session->SendNotification(
        "Revive failed (you must be in world with an active character).");
    return false;
  }
  return true;
}

bool CommandService::HandleDamage(std::shared_ptr<ICommandSession> session,
                                  const std::vector<std::string> &args,
                                  PrivilegeOrigin origin) {
  (void)origin;
  if (args.empty()) {
    session->SendNotification(
        "Usage: .damage <amount>  (select a player or NPC first)");
    return false;
  }
  uint64_t const targetGuid = session->GetClientSelectionGuid();
  if (targetGuid == 0) {
    session->SendNotification("Select a player or NPC, then use .damage <amount>.");
    return false;
  }

  try {
    unsigned long const raw = std::stoul(args[0]);
    if (raw == 0ul) {
      session->SendNotification("Damage amount must be greater than zero.");
      return false;
    }
    if (raw > static_cast<unsigned long>(std::numeric_limits<uint32_t>::max())) {
      session->SendNotification("Damage amount is too large.");
      return false;
    }
    uint32 const amount = static_cast<uint32>(raw);
    if (!session->GmDamageUnit(targetGuid, amount)) {
      session->SendNotification(
          "Damage failed (target not on your map, or you are not in world).");
      return false;
    }
    return true;
  } catch (const std::exception &) {
    session->SendNotification("Invalid damage amount (expected positive integer).");
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

bool CommandService::HandleCd(std::shared_ptr<ICommandSession> session,
                              const std::vector<std::string> &args,
                              PrivilegeOrigin origin) {
  (void)args;
  (void)origin;
  if (!session->GmResetAllCooldowns()) {
    session->SendNotification(
        "Cooldown reset failed (not in world).  Console: .cd <OnlineCharName>");
    return false;
  }
  session->SendNotification("All cooldowns reset (including racials).");
  return true;
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
        "Usage: .ticket queue | .ticket mine | .ticket ui | .ticket take <id> | "
        ".ticket reply <id> <message> | .ticket close <id>");
    return true;
  }

  std::string const sub = args[0];
  if (sub == "ui") {
    session->OpenGmTicketUi();
    return true;
  }
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

bool CommandService::HandleNpc(std::shared_ptr<ICommandSession> session,
                               const std::vector<std::string> &args,
                               PrivilegeOrigin origin) {
  if (args.empty()) {
    session->SendNotification(
        "Usage: .npc search [nameFragment]  |  .npc info  |  .npc add <creatureEntry> "
        "[displayId] [factionTemplate]  |  .npc del\n"
        "Administrator (access 3) only. `.npc search <fragment>` prints colored matches "
        "to system chat (no gossip).\n"
        "Console: .npc <OnlineChar> search [fragment]  |  same add/del patterns.\n"
        "Optional displayId defaults to " +
        std::to_string(kDefaultGmNpcDisplayId) +
        " when omitted. Optional factionTemplate: 0 reads `creature_template.faction` for "
        "that entry when the world DB exposes it.");
    return false;
  }

  if (AsciiEqualsLower(args[0], "info")) {
    if (!session->GmNpcPrintTargetInfo()) {
      session->SendNotification(
          "NPC info failed (select a creature on your map; opens a gossip window).");
      return false;
    }
    return true;
  }

  if (AsciiEqualsLower(args[0], "search")) {
    std::string const q = JoinArgs(args.begin() + 1, args.end());
    if (!session->GmNpcSearchPrintResults(q)) {
      session->SendNotification(
          "|cffff5555[NPC search]|r Failed (must be in world with creature_template "
          "configured).");
      return false;
    }
    return true;
  }

  if (AsciiEqualsLower(args[0], "add")) {
    if (args.size() < 2) {
      session->SendNotification(
          "Usage: .npc add <creatureEntry> [displayId] [factionTemplate]  (defaults "
          "displayId=" +
          std::to_string(kDefaultGmNpcDisplayId) +
          "; factionTemplate 0 = use `creature_template.faction` from world DB when "
          "available, else server fallback)");
      return false;
    }
    uint32 entry = 0;
    uint32 displayId = kDefaultGmNpcDisplayId;
    uint32 factionTemplate = 0;
    try {
      entry = static_cast<uint32>(std::stoul(args[1]));
      if (args.size() >= 3)
        displayId = static_cast<uint32>(std::stoul(args[2]));
      if (args.size() >= 4)
        factionTemplate = static_cast<uint32>(std::stoul(args[3]));
    } catch (const std::exception &) {
      session->SendNotification("Invalid creatureEntry, displayId, or factionTemplate.");
      return false;
    }
    if (entry == 0 || displayId == 0) {
      session->SendNotification("creatureEntry and displayId must be non-zero.");
      return false;
    }
    if (!session->GmSpawnNpc(entry, displayId, factionTemplate)) {
      session->SendNotification("NPC spawn failed (you must be in world on a character).");
      return false;
    }
    return true;
  }

  if (AsciiEqualsLower(args[0], "del")) {
    if (origin == PrivilegeOrigin::ServerConsole) {
      if (args.size() < 2) {
        session->SendNotification(
            "Usage (console): .npc <OnlineChar> del <creatureGuid>  (decimal guid "
            "from spawn message)");
        return false;
      }
      uint64 guid = 0;
      try {
        guid = std::stoull(args[1]);
      } catch (const std::exception &) {
        session->SendNotification("Invalid creatureGuid.");
        return false;
      }
      if (!session->GmDeleteNpcByObjectGuid(guid)) {
        session->SendNotification("NPC delete failed.");
        return false;
      }
      return true;
    }

    uint64 const sel = session->GetClientSelectionGuid();
    if (sel == 0) {
      session->SendNotification("Target an NPC in-game, then use .npc del.");
      return false;
    }
    if (!session->GmDeleteNpcByObjectGuid(sel)) {
      session->SendNotification(
          "NPC delete failed (selection is not a creature on your map).");
      return false;
    }
    return true;
  }

  session->SendNotification("Unknown .npc subcommand (use search, info, add, or del).");
  return false;
}

bool CommandService::HandleFaction(std::shared_ptr<ICommandSession> session,
                                   const std::vector<std::string> &args,
                                   PrivilegeOrigin origin) {
  (void)origin;
  if (args.size() < 2) {
    session->SendNotification(
        "Usage: .faction forced set <factionDbcId> <rank0-7>  |  .faction forced clear "
        "<factionDbcId>  |  .faction forced clearall  |  .faction template self "
        "<factionTemplate>  |  .faction template target <factionTemplate>\n"
        "Ranks: 0=hated 1=hostile 2=unfriendly 3=neutral 4=friendly 5=honored 6=revered "
        "7=exalted");
    return false;
  }

  if (AsciiEqualsLower(args[0], "forced")) {
    if (args.size() >= 2 && AsciiEqualsLower(args[1], "clearall")) {
      if (!session->GmClearAllForcedFactionReactions()) {
        session->SendNotification("Failed (must be in world on a character).");
        return false;
      }
      session->SendNotification("Forced faction reactions cleared.");
      return true;
    }
    if (args.size() >= 3 && AsciiEqualsLower(args[1], "clear")) {
      uint32 factionId = 0;
      try {
        factionId = static_cast<uint32>(std::stoul(args[2]));
      } catch (const std::exception &) {
        session->SendNotification("Invalid factionDbcId.");
        return false;
      }
      if (!session->GmClearForcedFactionReaction(factionId)) {
        session->SendNotification("Failed (must be in world on a character).");
        return false;
      }
      session->SendNotification("Forced reaction cleared for faction id " +
                                std::to_string(factionId) + ".");
      return true;
    }
    if (args.size() >= 4 && AsciiEqualsLower(args[1], "set")) {
      uint32 factionId = 0;
      uint32 rank = 0;
      try {
        factionId = static_cast<uint32>(std::stoul(args[2]));
        rank = static_cast<uint32>(std::stoul(args[3]));
      } catch (const std::exception &) {
        session->SendNotification("Invalid factionDbcId or rank.");
        return false;
      }
      if (rank > 7u) {
        session->SendNotification("rank must be 0..7.");
        return false;
      }
      if (!session->GmSetForcedFactionReaction(factionId,
                                              static_cast<uint8>(rank))) {
        session->SendNotification("Failed (must be in world on a character).");
        return false;
      }
      session->SendNotification("Forced reaction set: faction " +
                                std::to_string(factionId) + " rank " +
                                std::to_string(rank) + ".");
      return true;
    }
    session->SendNotification("Unknown .faction forced subcommand.");
    return false;
  }

  if (AsciiEqualsLower(args[0], "template")) {
    if (args.size() < 3) {
      session->SendNotification(
          "Usage: .faction template self <factionTemplate>  |  .faction template target "
          "<factionTemplate>");
      return false;
    }
    uint32 tpl = 0;
    try {
      tpl = static_cast<uint32>(std::stoul(args[2]));
    } catch (const std::exception &) {
      session->SendNotification("Invalid factionTemplate.");
      return false;
    }
    if (AsciiEqualsLower(args[1], "self")) {
      if (!session->GmSetOwnFactionTemplate(tpl)) {
        session->SendNotification("Failed (in world only; factionTemplate must be non-zero).");
        return false;
      }
      session->SendNotification("Player faction template set to " + std::to_string(tpl) +
                                ".");
      return true;
    }
    if (AsciiEqualsLower(args[1], "target")) {
      if (!session->GmSetSelectedCreatureFactionTemplate(tpl)) {
        session->SendNotification(
            "Failed (select a creature on this map; factionTemplate must be non-zero).");
        return false;
      }
      session->SendNotification("Creature faction template set to " + std::to_string(tpl) +
                                ".");
      return true;
    }
    session->SendNotification("Unknown .faction template mode (use self or target).");
    return false;
  }

  session->SendNotification("Unknown .faction branch (use forced or template).");
  return false;
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
