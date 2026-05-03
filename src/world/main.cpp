#include <application/services/CharacterService.h>
#include <application/services/CommandService.h>
#include <application/services/GmTicketService.h>
#include <application/services/OnlineCharacterSessionRegistry.h>
#include <application/services/PlayerCreateInfoService.h>
#include <application/services/WorldService.h>
#include <chrono>
#include <conncpp.hpp>
#include <infrastructure/network/asio/AsyncNetworkServer.h>
#include <infrastructure/network/realm_link/RealmLinkOutbound.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/persistence/DatabaseMigrator.h>
#include <infrastructure/persistence/MySqlAccountDataRepository.h>
#include <infrastructure/persistence/MySqlAccountRepository.h>
#include <infrastructure/persistence/MySqlCharacterRepository.h>
#include <infrastructure/persistence/MySqlPlayerCreateInfoRepository.h>
#include <infrastructure/persistence/MySqlGmTicketRepository.h>
#include <infrastructure/persistence/MySqlRealmRepository.h>
#include <infrastructure/scripting/LuaGameScriptHost.h>
#include <infrastructure/world/MapCollisionQueriesStub.h>
#include <atomic>
#include <memory>
#include <shared/Banner.h>
#include <shared/Config.h>
#include "WorldFtxuiConsole.h"
#include "WorldInteractiveConsole.h"
#include <shared/dbc/ItemDbHotfixStore.h>
#include <shared/dbc/LanguagesDbc.h>
#include <shared/dbc/SpellDbc.h>
#include <shared/Logger.h>
#include <thread>

using namespace Firelands;

int main(int argc, char **argv) {
  // Initial logging setup before config load
  Logger::Init(LoggerBuilder()
                   .WithName("firelands-world")
                   .WithConsole(true)
                   .WithConsoleLevel(LogLevel::Info)
                   .Build());

  if (!Config::LoadFromSearchPaths(
          "worldserver.yaml", (argc > 0) ? argv[0] : nullptr,
          "FIRELANDS_WORLD_CONFIG")) {
    LOG_ERROR("Could not find/load worldserver.yaml (cwd, exe parents, or "
              "FIRELANDS_WORLD_CONFIG); exiting.");
    return 1;
  }

  Config& config = Config::Instance();

  const bool stickyYaml =
      config.GetNestedBool({"Log", "StickyBanner"}, false);
  const bool stickyWant = ResolveStickyBanner(stickyYaml);
  if (stickyWant && !StdoutIsInteractiveTerminal()) {
    LOG_WARN(
        "Log.StickyBanner is enabled but stdout is not a TTY (pipe/redirect); "
        "using normal console layout.");
  }
  // When Console.Tui is on, the FTXUI screen owns the banner; skip stdout art
  // so startup does not flash plain terminal before fullscreen.
  bool consoleEnabledForBanner =
      config.GetNested<bool>({"Console", "Enabled"}, true);
  if (consoleEnabledForBanner && !StdoutIsInteractiveTerminal()) {
    consoleEnabledForBanner = false;
  }
  const bool useTerminalUiForBanner =
      consoleEnabledForBanner &&
      config.GetNested<bool>({"Console", "Tui"}, true);
  if (!useTerminalUiForBanner) {
    PrintBanner(BannerType::World, stickyWant);
  }

  // Re-initialize logger with config values
  Logger::Shutdown();
  Logger::Init(LoggerBuilder()
                   .WithName("firelands-world")
                   .WithConsole(true)
                   .WithConsoleLevel(config.GetNested<LogLevel>(
                       {"Log", "Level"}, LogLevel::Info))
                   .WithFile(true, config.GetNested<std::string>(
                       {"Log", "File"}, "logs/firelands-world.log"))
                   .WithFileLevel(LogLevel::Debug)
                   .WithRotatingFile(10 * 1024 * 1024, 5)
                   .Build());

  LOG_INFO("Starting World Server...");

  std::atomic<bool> stopRealmLink{false};
  std::unique_ptr<std::thread> realmLinkThread;
  if (config.GetNested<bool>({"RealmLink", "Enabled"}, false)) {
    realmLinkThread = std::make_unique<std::thread>([&config, &stopRealmLink] {
      RunRealmLinkOutbound(config, stopRealmLink);
    });
  }

  try {
    std::string scriptsRoot = config.GetNested<std::string>(
        {"Scripting", "ScriptsDirectory"}, "scripts/lua");
    auto scriptHost = std::make_shared<LuaGameScriptHost>();
    if (!scriptHost->Init(scriptsRoot)) {
      LOG_WARN("Lua script host failed to initialize (continuing without scripts)");
    } else {
      LOG_INFO("Lua script host ready, root: {}", scriptsRoot);
      scriptHost->FireEvent("world_startup", 0);
    }
    WorldService::Instance().SetScriptHost(std::move(scriptHost));

    const std::string collisionRoot =
        config.GetNested<std::string>({"Collision", "DataRoot"}, "");
    WorldService::Instance().SetCollisionQueries(
        std::make_shared<MapCollisionQueriesStub>(collisionRoot));

    // General DB Setup
    std::string dbUser =
        config.GetNested<std::string>({"Database", "User"}, "firelands");
    std::string dbPass =
        config.GetNested<std::string>({"Database", "Password"}, "firelands");

    std::string authUrl = config.GetNested<std::string>(
        {"Database", "Auth", "URI"},
        "jdbc:mariadb://localhost:3306/firelands_auth");

    // 1. Database Migration / Validation
    DatabaseMigrator::MigrateDirectory(authUrl, dbUser, dbPass, "sql");

    sql::Driver *driver = sql::mariadb::get_driver_instance();
    sql::Properties properties({{"user", dbUser}, {"password", dbPass}});

    std::shared_ptr<sql::Connection> authConn(
        driver->connect(authUrl, properties));

    // 3. Establish Character Database Connection
    std::string charUrl = config.GetNested<std::string>(
        {"Database", "Characters", "URI"},
        "jdbc:mariadb://localhost:3306/firelands_characters");

    std::shared_ptr<sql::Connection> charConn(
        driver->connect(charUrl, properties));

    // 4. Establish World Database Connection
    std::string worldUrl = config.GetNested<std::string>(
        {"Database", "World", "URI"},
        "jdbc:mariadb://localhost:3306/firelands_world");
    std::shared_ptr<sql::Connection> worldConn(
        driver->connect(worldUrl, properties));

    // 5. Initialize Repositories and Services
    auto accountRepo = std::make_shared<MySqlAccountRepository>(authConn);
    auto realmRepo = std::make_shared<MySqlRealmRepository>(authConn);
    auto authService = std::make_shared<AuthService>(accountRepo);

    auto accountDataRepo =
        std::make_shared<MySqlAccountDataRepository>(authConn, charConn);

    auto charRepo = std::make_shared<MySqlCharacterRepository>(charConn);
    auto playerCreateInfoRepo =
        std::make_shared<MySqlPlayerCreateInfoRepository>(worldConn);
    const std::string dbcBasePath =
        config.GetNested<std::string>({"Data", "DbcPath"}, "data/dbc");
    const std::string charStartOutfitDbcPath = dbcBasePath + "/CharStartOutfit.dbc";
    auto playerCreateInfoService = std::make_shared<PlayerCreateInfoService>(
        playerCreateInfoRepo, charStartOutfitDbcPath, dbcBasePath);
    auto charService =
        std::make_shared<CharacterService>(charRepo, playerCreateInfoService);
    auto onlineCharRegistry = std::make_shared<OnlineCharacterSessionRegistry>();
    auto gmTicketRepo = std::make_shared<MySqlGmTicketRepository>(charConn);
    auto gmTicketService =
        std::make_shared<GmTicketService>(gmTicketRepo, charService);
    auto commandService = std::make_shared<CommandService>(
        onlineCharRegistry, accountRepo, charService, gmTicketService);

    auto languagesDbc = std::make_shared<LanguagesDbc>();
    if (!languagesDbc->Load(dbcBasePath + "/Languages.dbc")) {
      LOG_WARN("Languages.dbc not loaded from {}; chat language validation "
               "falls back to spell table only.",
               dbcBasePath + "/Languages.dbc");
      languagesDbc.reset();
    }

    auto spellDbc = std::make_shared<SpellDbc>();
    if (!spellDbc->Load(dbcBasePath + "/Spell.dbc")) {
      LOG_WARN("Spell.dbc not loaded from {}; known spells are not filtered "
               "against client data.",
               dbcBasePath + "/Spell.dbc");
      spellDbc.reset();
    }

    auto itemDbHotfix = std::make_shared<ItemDbHotfixStore>();
    itemDbHotfix->load(dbcBasePath);

    auto sessionFactory = [authService, charService, commandService,
                           accountDataRepo, languagesDbc, spellDbc, realmRepo,
                           onlineCharRegistry, gmTicketService,
                           itemDbHotfix](boost::asio::ip::tcp::socket socket) {
      std::make_shared<WorldSession>(std::move(socket), authService, charService,
                                     commandService, accountDataRepo,
                                     languagesDbc, spellDbc, realmRepo,
                                     onlineCharRegistry, gmTicketService,
                                     itemDbHotfix)
          ->Start();
    };

    AsyncNetworkServer worldServer(sessionFactory);

    std::string bindIp = Config::Instance().GetNested<std::string>(
        {"Network", "BindAddress"}, "0.0.0.0");
    int port = Config::Instance().GetNested<int>({"Network", "Port"}, 8085);

    if (worldServer.Start(bindIp, port)) {
      LOG_INFO("World Server listening on {}:{}", bindIp, port);

      bool consoleEnabled =
          config.GetNested<bool>({"Console", "Enabled"}, true);
      if (consoleEnabled && !StdoutIsInteractiveTerminal()) {
        LOG_INFO("Console.Enabled is true but stdin is not a TTY; interactive "
                 "console disabled.");
        consoleEnabled = false;
      }

      WorldInteractiveConsole interactiveConsole(commandService);
      commandService->SetShutdownRequestHandler([&interactiveConsole]() {
        interactiveConsole.RequestShutdown();
      });
      const bool useTerminalUi =
          consoleEnabled &&
          config.GetNested<bool>({"Console", "Tui"}, true);

      if (useTerminalUi) {
        interactiveConsole.Start(consoleEnabled, false);
        LOG_INFO("Terminal UI (FTXUI): logs above, command input fixed below.");
        RunWorldFtxuiConsole(worldServer, interactiveConsole);
      } else if (consoleEnabled) {
        interactiveConsole.Start(consoleEnabled, true);
        LOG_INFO("Interactive console (stdin); type .help or quit to exit.");
        while (!interactiveConsole.ShutdownRequested()) {
          worldServer.Update();
          interactiveConsole.ProcessPending();
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      } else {
        while (!interactiveConsole.ShutdownRequested()) {
          worldServer.Update();
          commandService->PollScheduledRestart();
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      }
      stopRealmLink.store(true);
      LOG_INFO("World server main loop stopped.");
      LOG_INFO("World server shutting down...");
      WorldService::Instance().ResetForShutdown();
      worldServer.Stop();
      if (realmLinkThread && realmLinkThread->joinable()) {
        realmLinkThread->join();
      }
      commandService->SetShutdownRequestHandler({});
    } else {
      LOG_CRITICAL("Failed to start World Server.");
      Logger::Shutdown();
      return 1;
    }

  } catch (std::exception &e) {
    LOG_CRITICAL("Fatal error: {}", e.what());
    Logger::Shutdown();
    return 1;
  }

  Logger::Shutdown();
  return 0;
}
