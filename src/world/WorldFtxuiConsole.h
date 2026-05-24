#pragma once

#include <domain/world/MapSnapshot.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace Firelands {

class AsyncNetworkServer;
class WorldInteractiveConsole;
class CommandService;

/// Cached map-status strip for the world TUI (refreshed on the ticker thread).
struct MapStatusPanelModel {
  std::vector<MapSnapshot> lines;
  std::size_t hidden_map_count = 0;
  int screen_rows = 2;
};

/// Lightweight watch state — full panel rebuild only when this changes.
struct MapStatusWatchState {
  std::size_t registered_map_count = 0;
  std::vector<std::pair<uint32, int>> player_count_by_map;

  bool operator==(MapStatusWatchState const &other) const {
    return registered_map_count == other.registered_map_count &&
           player_count_by_map == other.player_count_by_map;
  }
};

/// Shared state between the FTXUI main thread (render + tick) and the
/// background bootstrap thread that builds network services.
struct WorldFtxuiRuntime {
  std::mutex mutex;
  bool bootstrap_failed = false;
  bool services_ready = false;
  std::shared_ptr<AsyncNetworkServer> world_server;
  std::shared_ptr<WorldInteractiveConsole> interactive_console;
  std::shared_ptr<CommandService> command_service;

  std::mutex map_status_mutex;
  MapStatusPanelModel map_status;
  MapStatusWatchState map_status_watch;
};

/// Full-screen terminal UI (FTXUI): scrollable log pane and a fixed bottom
/// command strip. `bootstrap_worker` runs on a background thread while the UI
/// is already active so startup logs appear in the log pane immediately.
/// Replaces the stdout color sink for the duration of the run.
void RunWorldFtxuiConsole(
    std::shared_ptr<WorldFtxuiRuntime> runtime,
    std::function<void(std::shared_ptr<WorldFtxuiRuntime>)> bootstrap_worker);

} // namespace Firelands
