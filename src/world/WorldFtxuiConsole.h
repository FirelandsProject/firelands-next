#pragma once

namespace Firelands {

class AsyncNetworkServer;
class WorldInteractiveConsole;

/// Full-screen terminal UI (FTXUI): scrollable log pane and a fixed bottom
/// command strip. Replaces the stdout color sink for the duration of the run.
/// Returns when the user stops the server (`quit` / `exit`) or the UI exits.
void RunWorldFtxuiConsole(AsyncNetworkServer &worldServer,
                          WorldInteractiveConsole &interactiveConsole);

} // namespace Firelands
