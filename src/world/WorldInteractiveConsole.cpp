#include "WorldInteractiveConsole.h"
#include <application/ports/ICommandService.h>
#include <shared/Common.h>
#include <shared/game/AccessLevel.h>
#include <shared/network/MovementInfo.h>
#include <shared/Logger.h>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#ifndef _WIN32
#include <poll.h>
#include <unistd.h>
#endif
#ifdef _WIN32
#include <io.h>
#endif

namespace Firelands {

namespace {

class ServerConsoleCommandSession final : public ICommandSession {
  MovementInfo _position{};

public:
  void SendNotification(const std::string &message) override {
    // INFO so output appears in the default console/TUI log pane (debug is often
    // filtered) and matches user expectations for REPL feedback.
    LOG_INFO("[console] {}", message);
  }

  const MovementInfo &GetPosition() const override { return _position; }

  uint32 GetMapId() const override { return 0; }

  void TeleportTo(uint32_t /*mapId*/, float /*x*/, float /*y*/, float /*z*/,
                  float /*orientation*/) override {
    LOG_WARN("[console] Use: .tele <OnlineCharName> x y z [mapId] (this stub "
             "session has no character).");
  }

  void RequestDisconnect(std::string const &reason) override {
    LOG_DEBUG("[console] RequestDisconnect (no world character): {}", reason);
  }

  void SendPacket(WorldPacket &packet) override { (void)packet; }

  bool GmLearnSpell(uint32 /*spellId*/) override { return false; }

  bool GmUnlearnSpell(uint32 /*spellId*/) override { return false; }

  bool GmModifyMoneyCopper(int64 /*delta*/) override { return false; }

  bool GmAddItem(uint32 /*itemEntry*/, uint32 /*count*/) override { return false; }

  bool GmRemoveItem(uint32 /*itemEntry*/, uint32 /*count*/) override { return false; }

  bool GmSetLevel(uint8 /*level*/) override { return false; }

  uint64_t GetClientSelectionGuid() const override { return 0; }

  uint64_t GetActiveCharacterObjectGuid() const override { return 0; }

  AccessLevel GetAccountAccessLevel() const override {
    return AccessLevel::Player;
  }
};

std::string TrimInPlace(std::string s) {
  while (!s.empty() &&
         (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' ||
          s.back() == '\t')) {
    s.pop_back();
  }
  size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
    ++i;
  }
  return s.substr(i);
}

bool IEqualsAscii(std::string const &a, char const *b) {
  if (a.size() != std::strlen(b))
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        static_cast<unsigned char>(b[i])) {
      return false;
    }
  }
  return true;
}

bool EnvironmentRequestsPlainConsole() {
  if (std::getenv("NO_COLOR") != nullptr) {
    return true;
  }
  const char *p = std::getenv("FIRELANDS_PLAIN_CONSOLE");
  return p != nullptr && p[0] == '1' && p[1] == '\0';
}

bool StdoutStdinLookInteractive() {
#ifndef _WIN32
  return ::isatty(STDOUT_FILENO) != 0 && ::isatty(STDIN_FILENO) != 0;
#else
  return ::_isatty(::_fileno(stdout)) != 0 && ::_isatty(::_fileno(stdin)) != 0;
#endif
}

/// Reserved full-width input rail (OpenCode-style): left ember accent, dark
/// panel, dim hint, chevron, then clear-to-EOL so the rest of the row is the
/// typing canvas. Typed text appears after the chevron (canonical tty echo).
void WriteReservedCommandInputRail(bool useAnsi) {
  std::ostream &out = std::cout;
  if (!useAnsi) {
    out << "world> ";
    out.flush();
    return;
  }
  // Firelands palette: ember accent (OpenCode blue bar analogue), charcoal panel.
  constexpr char esc = '\033';
  out << esc << "[0m\n"
      << esc << "[48;2;255;115;58m " // 1-col vertical accent
      << esc << "[48;2;26;24;23m"
      << esc << "[2m" << esc << "[38;2;118;112;106m"
      << " .help " << esc << "[22m"
      << esc << "[38;2;240;155;88m" << "▸" << esc << "[38;2;248;242;232m"
      << " " << esc << "[K"; // fill row to terminal edge with current panel bg
  out.flush();
}

void FinishReservedCommandInputRail(bool useAnsi) {
  if (!useAnsi) {
    return;
  }
  std::cout << "\033[0m";
  std::cout.flush();
}

} // namespace

WorldInteractiveConsole::WorldInteractiveConsole(
    std::shared_ptr<ICommandService> commands)
    : _commands(std::move(commands)),
      _consoleSession(std::make_shared<ServerConsoleCommandSession>()) {}

WorldInteractiveConsole::~WorldInteractiveConsole() {
  _stopReader = true;
  if (_reader && _reader->joinable()) {
    _reader->join();
  }
}

void WorldInteractiveConsole::RequestShutdown() {
  _shutdownRequested = true;
  LOG_INFO("[console] shutdown requested; stopping world loop.");
}

void WorldInteractiveConsole::Start(bool enabled, bool useStdinReader) {
  if (!enabled || !_commands) {
    _enabled = false;
    return;
  }
  _enabled = true;
  _useAnsiCommandRail = useStdinReader && !EnvironmentRequestsPlainConsole() &&
                        StdoutStdinLookInteractive();
#ifdef _WIN32
  _useAnsiCommandRail = false;
#endif
  if (useStdinReader) {
    _reader = std::make_unique<std::thread>([this] { ReaderLoop(); });
  }
}

void WorldInteractiveConsole::SubmitLine(std::string line) {
  if (!_enabled.load()) {
    return;
  }
  std::lock_guard<std::mutex> lock(_mutex);
  _lines.push_back(std::move(line));
}

void WorldInteractiveConsole::ReaderLoop() {
#ifndef _WIN32
  while (!_stopReader.load()) {
    struct pollfd pfd {};
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    int const pr = poll(&pfd, 1, 250);
    if (pr < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    if (pr == 0) {
      continue;
    }
    if ((pfd.revents & (POLLIN | POLLHUP | POLLERR)) == 0) {
      continue;
    }
    WriteReservedCommandInputRail(_useAnsiCommandRail);
    std::string line;
    if (!std::getline(std::cin, line)) {
      FinishReservedCommandInputRail(_useAnsiCommandRail);
      break;
    }
    FinishReservedCommandInputRail(_useAnsiCommandRail);
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _lines.push_back(std::move(line));
    }
  }
#else
  while (!_stopReader.load()) {
    WriteReservedCommandInputRail(false);
    std::string line;
    if (!std::getline(std::cin, line)) {
      break;
    }
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _lines.push_back(std::move(line));
    }
  }
#endif
}

void WorldInteractiveConsole::ProcessPending() {
  if (_commands) {
    _commands->PollScheduledRestart();
  }
  if (!_enabled.load()) {
    return;
  }
  std::vector<std::string> batch;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    batch.swap(_lines);
  }
  for (std::string &raw : batch) {
    std::string line = TrimInPlace(std::move(raw));
    if (line.empty()) {
      continue;
    }
    if (IEqualsAscii(line, "quit") || IEqualsAscii(line, "exit")) {
      RequestShutdown();
      continue;
    }
    // Same as bare quit/exit; users often type .exit after .help.
    if (line.size() >= 2 && line[0] == '.') {
      std::string const inner = TrimInPlace(line.substr(1));
      if (IEqualsAscii(inner, "quit") || IEqualsAscii(inner, "exit")) {
        RequestShutdown();
        continue;
      }
    }
    if (!_commands->IsCommand(line)) {
      LOG_DEBUG("[console] not a command (use .help). Ignored: '{}'",
                line.size() > 64 ? line.substr(0, 64) + "..." : line);
      continue;
    }
    _commands->ExecuteCommand(_consoleSession, line,
                              PrivilegeOrigin::ServerConsole);
  }
}

} // namespace Firelands
