#include "WorldFtxuiConsole.h"

#include <application/services/MapService.h>
#include <application/services/WorldService.h>
#include <domain/world/MapSnapshot.h>
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
#include <shared/tui/FtxuiTextClipboard.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Firelands {
namespace {

using namespace ftxui;

int constexpr kWorldBottomChromeRows = 6;
/// Max map lines in the status strip (fixed height so logs keep space).
int constexpr kMapStatusMaxLines = 4;

bool IsContinentMapId(uint32 mapId) {
  return mapId == 0u || mapId == 1u || mapId == 530u;
}

bool IncludeInMapStatusPanel(MapSnapshot const &snap) {
  return snap.playerCount > 0 || IsContinentMapId(snap.mapId);
}

std::string_view MapDisplayName(uint32 mapId) {
  switch (mapId) {
  case 0:
    return "Eastern Kingdoms";
  case 1:
    return "Kalimdor";
  case 530:
    return "Outland";
  default:
    return "Map";
  }
}

int MapStatusPanelScreenRows(MapStatusPanelModel const &model) {
  int rows = 1;
  if (model.lines.empty())
    return rows + 1;
  rows += static_cast<int>(model.lines.size());
  if (model.hidden_map_count > 0)
    ++rows;
  return rows;
}

void FinalizeMapStatusPanelModel(MapStatusPanelModel &model,
                                 std::size_t loaded_map_count) {
  if (model.lines.size() > static_cast<std::size_t>(kMapStatusMaxLines)) {
    model.hidden_map_count = model.lines.size() - kMapStatusMaxLines;
    model.lines.resize(static_cast<std::size_t>(kMapStatusMaxLines));
  }
  if (model.lines.empty() && loaded_map_count > 0) {
    model.hidden_map_count = loaded_map_count;
  }
  model.screen_rows = MapStatusPanelScreenRows(model);
}

/// Called from the ticker thread only — snapshots a subset of maps (not every frame).
MapStatusPanelModel BuildMapStatusPanelModelFromWorld() {
  std::vector<MapSnapshot> selected;
  std::size_t loaded_map_count = 0;
  WorldService::Instance().ForEachMapService([&](MapService &svc) {
    ++loaded_map_count;
    auto const map = svc.SharedMap();
    if (!map) {
      return;
    }
    uint32 const mapId = svc.MapId();
    if (!IsContinentMapId(mapId) && map->IsEmpty()) {
      return;
    }
    selected.push_back(svc.Snapshot());
  });

  std::sort(selected.begin(), selected.end(),
            [](MapSnapshot const &a, MapSnapshot const &b) {
              auto const rank = [](MapSnapshot const &s) {
                if (s.playerCount > 0)
                  return 0;
                if (IsContinentMapId(s.mapId))
                  return 1;
                return 2;
              };
              int const ra = rank(a);
              int const rb = rank(b);
              if (ra != rb)
                return ra < rb;
              if (a.playerCount != b.playerCount)
                return a.playerCount > b.playerCount;
              return a.mapId < b.mapId;
            });

  MapStatusPanelModel model;
  model.lines = std::move(selected);
  FinalizeMapStatusPanelModel(model, loaded_map_count);
  return model;
}

void RefreshMapStatusCache(std::shared_ptr<WorldFtxuiRuntime> const &runtime) {
  MapStatusPanelModel const model = BuildMapStatusPanelModelFromWorld();
  std::lock_guard<std::mutex> lock(runtime->map_status_mutex);
  runtime->map_status = model;
}

MapStatusPanelModel CopyMapStatusPanelModel(
    std::shared_ptr<WorldFtxuiRuntime> const &runtime) {
  std::lock_guard<std::mutex> lock(runtime->map_status_mutex);
  return runtime->map_status;
}

FtxuiLogViewLayout WorldLogLayoutWithMapStatus(FtxuiLogViewLayout base,
                                               MapStatusPanelModel const &model) {
  base.middle_top_rows = model.screen_rows + 1;
  return base;
}

Element RenderMapStatusPanel(MapStatusPanelModel const &model) {
  Elements lines;
  lines.push_back(text("Map Status:") | bold | color(FtxuiServerPalette::Accent()));

  if (model.lines.empty()) {
    if (model.hidden_map_count > 0) {
      lines.push_back(text(std::format("  {} maps loaded, no players online",
                                       model.hidden_map_count)) |
                      dim);
    } else {
      lines.push_back(text("  (no maps)") | dim);
    }
    return vbox(std::move(lines)) | size(HEIGHT, EQUAL, model.screen_rows);
  }

  for (MapSnapshot const &snap : model.lines) {
    std::string suffix;
    if (snap.isEmpty)
      suffix = " [empty]";
    std::string const line = std::format(
        "  {} ({}) Players: {} Tick: {:.1f}ms Grids: {}{}",
        MapDisplayName(snap.mapId), snap.mapId, snap.playerCount,
        snap.avgTickTimeMs, snap.loadedGridCells, suffix);
    lines.push_back(text(line) | color(Color::RGB(200, 190, 180)));
  }
  if (model.hidden_map_count > 0) {
    lines.push_back(text(std::format("  … and {} more maps hidden", model.hidden_map_count)) |
                    dim);
  }
  return vbox(std::move(lines)) | size(HEIGHT, EQUAL, model.screen_rows);
}

void RunWorldFtxuiConsoleImpl(
    std::shared_ptr<WorldFtxuiRuntime> runtime,
    std::function<void(std::shared_ptr<WorldFtxuiRuntime>)> bootstrap_worker) {
  IgnoreSigIntForTui const ignoreSigIntDuringTui;
  auto screen = ScreenInteractive::Fullscreen();

  FtxuiLogSinkPtr const log_sink = std::make_shared<FtxuiLogSink>(12000);
  BindFirelandsLoggerToFtxuiSink(log_sink);

  FtxuiLogViewLayout log_layout{};
  log_layout.banner_screen_rows = 11;
  FtxuiLogView log_view(log_layout, log_sink);

  std::thread bootstrap_thread([fn = std::move(bootstrap_worker), runtime]() {
    fn(runtime);
  });

  std::string command;
  int command_cursor = 0;
  Color const kInputBg = FtxuiServerPalette::InputBg();
  Color const kInputBgHover = FtxuiServerPalette::InputBgHover();
  Color const kInputBgFocus = FtxuiServerPalette::InputBgFocus();
  Color const kInputFg = FtxuiServerPalette::InputFg();
  InputOption input_opts = InputOption::Default();
  input_opts.multiline() = false;
  input_opts.cursor_position = &command_cursor;
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
  input = AttachInputClipboard(std::move(input), command, command_cursor,
                               {.multiline = false});
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

  Color const kShellBg = FtxuiServerPalette::ShellBg();

  auto refresh_log_layout = [&]() {
    MapStatusPanelModel const map_status = CopyMapStatusPanelModel(runtime);
    FtxuiLogViewLayout const layout =
        WorldLogLayoutWithMapStatus(log_layout, map_status);
    log_view.SetLayout(layout);
    int const log_h =
        ComputeFtxuiLogViewportHeight(kWorldBottomChromeRows, layout);
    return log_h;
  };

  auto request_frame = [&] { screen.RequestAnimationFrame(); };

  auto top_chrome = Renderer([&, runtime] {
    MapStatusPanelModel const map_status = CopyMapStatusPanelModel(runtime);
    return vbox({
               FirelandsTuiBanner("WORLD SERVER") | notflex,
               separator() | color(FtxuiServerPalette::Separator()),
               RenderMapStatusPanel(map_status) | notflex,
               separator() | color(FtxuiServerPalette::Separator()),
           }) |
           bgcolor(kShellBg);
  });

  auto log_area = Renderer([&] {
    int const log_h = refresh_log_layout();
    return log_view.Window(log_h);
  });
  log_area |= CatchEvent([&](Event e) {
    int const log_h = refresh_log_layout();
    return log_view.HandleEvent(e, log_h, request_frame);
  });

  auto bottom_chrome = Renderer(input, [&, runtime] {
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
               separator() | color(FtxuiServerPalette::Separator()),
               input_rail,
           }) |
           bgcolor(kShellBg);
  });

  auto shell = Container::Vertical({top_chrome, log_area, bottom_chrome});

  shell |= CatchEvent([&](Event e) {
    int const log_h = refresh_log_layout();
    if (e == Event::PageUp || e == Event::PageDown) {
      return log_view.HandleEvent(e, log_h, request_frame);
    }
    if (e.is_mouse()) {
      Mouse const &m = e.mouse();
      if (m.button == Mouse::WheelUp || m.button == Mouse::WheelDown) {
        return log_view.HandleEvent(e, log_h, request_frame);
      }
    }
    if (e.is_character() && e.character().size() == 1 &&
        static_cast<unsigned char>(e.character()[0]) == 3) {
      return true; // Ctrl+C: do not exit fullscreen / restore cooked tty
    }
    return false;
  });

  auto root = Renderer(shell, [&] { return shell->Render(); });

  RefreshMapStatusCache(runtime);

  std::atomic<bool> run_ticks{true};
  std::thread ticker([&, runtime] {
    auto const auraTickInterval = std::chrono::milliseconds(100);
    auto const mapStatusInterval = std::chrono::milliseconds(500);
    auto lastAuraTick = std::chrono::steady_clock::now();
    auto lastMapStatusRefresh = std::chrono::steady_clock::now();

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
        if (tickNow - lastMapStatusRefresh >= mapStatusInterval) {
          RefreshMapStatusCache(runtime);
          lastMapStatusRefresh = tickNow;
          screen.RequestAnimationFrame();
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
