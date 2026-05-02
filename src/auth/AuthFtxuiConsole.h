#pragma once

namespace Firelands {

class AsyncNetworkServer;
class RestAuthServer;

/// Full-screen log TUI (FTXUI) for the auth server: same banner style as the
/// world console, scrollable logs, no command line. Exit with **Q** or
/// **Ctrl+C** (FTXUI restores the terminal).
void RunAuthFtxuiConsole(AsyncNetworkServer &authTcpServer,
                         AsyncNetworkServer *realmLinkTcpServer);

} // namespace Firelands
