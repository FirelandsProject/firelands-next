#pragma once

#include <application/ports/ICommandSession.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Firelands {

class ICommandService;

/// Background stdin reader plus a queue drained from the main world loop.
/// Commands use the same prefix as in-game (`.` or `!`) and run with
/// `PrivilegeOrigin::ServerConsole`. Lines `quit` / `exit` request process
/// shutdown.
class WorldInteractiveConsole {
public:
  explicit WorldInteractiveConsole(std::shared_ptr<ICommandService> commands);
  ~WorldInteractiveConsole();

  WorldInteractiveConsole(const WorldInteractiveConsole &) = delete;
  WorldInteractiveConsole &operator=(const WorldInteractiveConsole &) = delete;

  /// @param styledPrompt Used only when @p useStdinReader is true: ANSI input
  ///        rail before each `getline`. Ignored for TUI mode.
  /// @param useStdinReader When false, only the command queue is enabled (TUI
  ///        submits lines via `SubmitLine`).
  void Start(bool enabled, bool styledPrompt = true,
             bool useStdinReader = true);
  void SubmitLine(std::string line);
  void ProcessPending();
  bool ShutdownRequested() const { return _shutdownRequested.load(); }

private:
  void ReaderLoop();

  std::shared_ptr<ICommandService> _commands;
  std::shared_ptr<ICommandSession> _consoleSession;

  std::atomic<bool> _enabled{false};
  std::atomic<bool> _stopReader{false};
  std::atomic<bool> _shutdownRequested{false};
  bool _styledPrompt = false;
  std::mutex _mutex;
  std::vector<std::string> _lines;
  std::unique_ptr<std::thread> _reader;
};

} // namespace Firelands
