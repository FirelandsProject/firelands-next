#include "WorldFtxuiConsole.h"

#include <infrastructure/world/MapAuraTicker.h>
#include <infrastructure/world/MapPlayerRegenTicker.h>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "WorldInteractiveConsole.h"
#include <infrastructure/network/asio/AsyncNetworkServer.h>
#include <application/services/CommandService.h>
#include <shared/tui/FtxuiBanner.h>
#include <shared/tui/FtxuiLogSink.h>
#include <shared/tui/FtxuiLogSpdlog.h>
#include <shared/tui/FtxuiLogView.h>
#include <shared/tui/FtxuiPalette.h>
#include <shared/tui/FtxuiSigInt.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace Firelands {
namespace {

using namespace ftxui;

int constexpr kWorldBottomChromeRows = 6;

void RunWorldFtxuiConsoleImpl(
    std::shared_ptr<WorldFtxuiRuntime> runtime,
    std::function<void(std::shared_ptr<WorldFtxuiRuntime>)> bootstrap_worker) {
  IgnoreSigIntForTui const ignoreSigIntDuringTui;
  auto screen = ScreenInteractive::Fullscreen();

  FtxuiLogSinkPtr const log_sink = std::make_shared<FtxuiLogSink>(12000);
  ReplaceStdoutColorSinkWith(log_sink);

  FtxuiLogViewLayout log_layout{};
  log_layout.banner_screen_rows = 11;
  FtxuiLogView log_view(log_layout, log_sink);

  std::thread bootstrap_thread([fn = std::move(bootstrap_worker), runtime]() {
    fn(runtime);
  });

  std::string command;
  Color const kInputBg = FtxuiServerPalette::InputBg();
  Color const kInputBgHover = FtxuiServerPalette::InputBgHover();
  Color const kInputBgFocus = FtxuiServerPalette::InputBgFocus();
  Color const kInputFg = FtxuiServerPalette::InputFg();
  InputOption input_opts = InputOption::Default();
  input_opts.multiline() = false;
  input_opts.transform = [=](InputState state) {
    Element e = std::move(state.element);
    Color bg = kInputBg;
    if (state.focused) {
      bg = kInputBgFocus;
    } else if (state.hovered) {
      bg = kInputBgHover;
    }
    e |= bgcolor(bg);
    e |= color(kInputFg);
    if (state.is_placeholder) {
      e |= dim;
    }
    return e;
  };
  auto input = Input(&command, " .help   .exit / quit ", input_opts);
  input |= CatchEvent([&, runtime](Event e) {
    if (e != Event::Return) {
      return false;
    }
    std::shared_ptr<WorldInteractiveConsole> ic;
    {
      std::lock_guard<std::mutex> lock(runtime->mutex);
      ic = runtime->interactive_console;
    }
    if (!ic) {
      return true;
    }
    std::string c = command;
    command.clear();
    while (!c.empty() && (c.back() == ' ' || c.back() == '\t')) {
      c.pop_back();
    }
    size_t i = 0;
    while (i < c.size() && (c[i] == ' ' || c[i] == '\t')) {
      ++i;
    }
    c = c.substr(i);
    if (!c.empty()) {
      ic->SubmitLine(std::move(c));
    }
    return true;
  });

  auto container = Container::Vertical({input});

  container |= CatchEvent([&](Event e) {
    int const log_h =
        ComputeFtxuiLogViewportHeight(kWorldBottomChromeRows);
    if (log_view.HandleEvent(e, log_h,
                             [&] { screen.RequestAnimationFrame(); })) {
      return true;
    }
    if (e.is_character() && e.character().size() == 1 &&
        static_cast<unsigned char>(e.character()[0]) == 3) {
      return true; // Ctrl+C: do not exit fullscreen / restore cooked tty
    }
    return false;
  });

  Color const kShellBg = FtxuiServerPalette::ShellBg();

  auto root = Renderer(container, [&, runtime] {
    int const log_h =
        ComputeFtxuiLogViewportHeight(kWorldBottomChromeRows);

    std::shared_ptr<WorldInteractiveConsole> ic;
    {
      std::lock_guard<std::mutex> lock(runtime->mutex);
      ic = runtime->interactive_console;
    }
    Element input_rail;
    if (!ic) {
      input_rail = hbox({
          text(" ") | bgcolor(FtxuiServerPalette::Accent()),
          text(" Initializing server… ") | dim | color(Color::RGB(200, 190, 180)) |
              flex | borderStyled(ROUNDED, Color::RGB(100, 90, 82)),
      });
    } else {
      input_rail = hbox({
          text(" ") | bgcolor(FtxuiServerPalette::Accent()),
          input->Render() | flex |
              borderStyled(ROUNDED, Color::RGB(100, 90, 82)),
      });
    }

    return vbox({
               FirelandsTuiBanner("WORLD SERVER") | notflex,
               separator() | color(FtxuiServerPalette::Separator()),
               log_view.Window(log_h),
               filler() | flex,
               separator() | color(FtxuiServerPalette::Separator()),
               input_rail,
           }) |
           bgcolor(kShellBg);
  });

  std::atomic<bool> run_ticks{true};
  std::thread ticker([&, runtime] {
    auto const auraTickInterval = std::chrono::milliseconds(100);
    auto lastAuraTick = std::chrono::steady_clock::now();

    while (run_ticks.load()) {
      bool failed = false;
      bool ready = false;
      std::shared_ptr<AsyncNetworkServer> ws;
      std::shared_ptr<WorldInteractiveConsole> ic;
      std::shared_ptr<CommandService> cs;
      {
        std::lock_guard<std::mutex> lock(runtime->mutex);
        failed = runtime->bootstrap_failed;
        ready = runtime->services_ready;
        if (ready && !failed) {
          ws = runtime->world_server;
          ic = runtime->interactive_console;
          cs = runtime->command_service;
        }
      }
      if (failed) {
        screen.ExitLoopClosure()();
      } else if (ready && ws) {
        ws->Update();
        if (ic) {
          ic->ProcessPending();
        }
        if (cs) {
          cs->PollScheduledRestart();
        }
        auto const tickNow = std::chrono::steady_clock::now();
        if (tickNow - lastAuraTick >= auraTickInterval) {
          TickMapAuras(tickNow);
          TickMapPlayerResourceRegen(tickNow);
          lastAuraTick = tickNow;
        }
        if (ic && ic->ShutdownRequested()) {
          screen.ExitLoopClosure()();
        }
      }
      if (log_sink->ConsumeRenderDirty()) {
        screen.Post(Event::Custom);
        screen.RequestAnimationFrame();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });

  screen.Loop(root);
  run_ticks = false;
  if (ticker.joinable()) {
    ticker.join();
  }
  if (bootstrap_thread.joinable()) {
    bootstrap_thread.join();
  }

  RestoreStdoutColorSink(log_sink);
  bool mute = false;
  {
    std::lock_guard<std::mutex> lock(runtime->mutex);
    if (runtime->interactive_console &&
        runtime->interactive_console->ShutdownRequested()) {
      mute = true;
    }
  }
  if (mute) {
    MuteTerminalLogSinks();
  }
}

} // namespace

void RunWorldFtxuiConsole(
    std::shared_ptr<WorldFtxuiRuntime> runtime,
    std::function<void(std::shared_ptr<WorldFtxuiRuntime>)> bootstrap_worker) {
  RunWorldFtxuiConsoleImpl(std::move(runtime), std::move(bootstrap_worker));
}

} // namespace Firelands
