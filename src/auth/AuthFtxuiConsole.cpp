#include "AuthFtxuiConsole.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <infrastructure/network/asio/AsyncNetworkServer.h>
#include <infrastructure/network/rest/RestAuthServer.h>
#include <shared/tui/FtxuiBanner.h>
#include <shared/tui/FtxuiLogSink.h>
#include <shared/tui/FtxuiLogSpdlog.h>
#include <shared/tui/FtxuiLogView.h>
#include <shared/tui/FtxuiPalette.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace Firelands {
namespace {

using namespace ftxui;

int constexpr kAuthBottomChromeRows = 3;

void RunAuthFtxuiConsoleImpl(
    std::shared_ptr<AuthFtxuiRuntime> runtime,
    std::function<void(std::shared_ptr<AuthFtxuiRuntime>)> bootstrap_worker) {
  auto screen = ScreenInteractive::Fullscreen();
  Closure const requestExit = screen.ExitLoopClosure();

  FtxuiLogSinkPtr const log_sink = std::make_shared<FtxuiLogSink>(12000);
  ReplaceStdoutColorSinkWith(log_sink);

  FtxuiLogViewLayout log_layout{};
  log_layout.banner_screen_rows = 11;
  FtxuiLogView log_view(log_layout, log_sink);

  std::thread bootstrap_thread([fn = std::move(bootstrap_worker), runtime]() {
    fn(runtime);
  });

  auto key_sink = Container::Vertical({});

  key_sink |= CatchEvent([&](Event e) {
    int const log_h =
        ComputeFtxuiLogViewportHeight(kAuthBottomChromeRows);
    if (log_view.HandleEvent(e, log_h,
                             [&] { screen.RequestAnimationFrame(); })) {
      return true;
    }
    if (!e.is_character() || e.character().size() != 1) {
      return false;
    }
    char const c = e.character()[0];
    if (c == 'q' || c == 'Q') {
      requestExit();
      return true;
    }
    return false;
  });

  Color const kShellBg = FtxuiServerPalette::ShellBg();

  auto root = Renderer(key_sink, [&] {
    int const log_h =
        ComputeFtxuiLogViewportHeight(kAuthBottomChromeRows);

    Element const footer = hbox({
        text(" ") | bgcolor(FtxuiServerPalette::Accent()),
        text("  Q  quit  ") | bold | color(Color::RGB(248, 242, 232)),
        text("  ·  Ctrl+C  stop  ") | dim | color(Color::RGB(180, 170, 160)),
        filler() | bgcolor(Color::RGB(36, 34, 32)),
    });

    return vbox({
               FirelandsTuiBanner("AUTH SERVER") | notflex,
               separator() | color(FtxuiServerPalette::Separator()),
               log_view.Window(log_h),
               filler() | flex,
               separator() | color(FtxuiServerPalette::Separator()),
               footer,
           }) |
           bgcolor(kShellBg);
  });

  std::atomic<bool> run_ticks{true};
  std::thread ticker([&, runtime] {
    while (run_ticks.load()) {
      bool failed = false;
      bool ready = false;
      std::shared_ptr<AsyncNetworkServer> auth_srv;
      std::shared_ptr<AsyncNetworkServer> realm_srv;
      {
        std::lock_guard<std::mutex> lock(runtime->mutex);
        failed = runtime->bootstrap_failed;
        ready = runtime->services_ready;
        if (ready && !failed) {
          auth_srv = runtime->auth_server;
          realm_srv = runtime->realm_link_server;
        }
      }
      if (failed) {
        requestExit();
      } else if (ready && auth_srv) {
        auth_srv->Update();
        if (realm_srv) {
          realm_srv->Update();
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
  MuteTerminalLogSinks();
}

} // namespace

void RunAuthFtxuiConsole(
    std::shared_ptr<AuthFtxuiRuntime> runtime,
    std::function<void(std::shared_ptr<AuthFtxuiRuntime>)> bootstrap_worker) {
  RunAuthFtxuiConsoleImpl(std::move(runtime), std::move(bootstrap_worker));
}

} // namespace Firelands
