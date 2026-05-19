#include "WorldApplication.h"

#include "WorldFtxuiConsole.h"
#include "WorldInteractiveConsole.h"
#include <application/services/CharacterService.h>
#include <application/services/CommandService.h>
#include <application/services/GmTicketService.h>
#include <application/services/OnlineCharacterSessionRegistry.h>
#include <application/services/PlayerCreateInfoService.h>
#include <application/services/WorldService.h>
#include <application/spell/SpellManager.h>
#include <atomic>
#include <chrono>
#include <conncpp.hpp>
#include <domain/repositories/ISpellCastTables.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <infrastructure/dbc/SpellCastTablesDbc.h>
#include <infrastructure/dbc/SpellEntryDbcStore.h>
#include <infrastructure/network/asio/AsyncNetworkServer.h>
#include <infrastructure/network/realm_link/RealmLinkOutbound.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/persistence/DatabaseMigrator.h>
#include <infrastructure/persistence/MySqlAccountDataRepository.h>
#include <infrastructure/persistence/MySqlAccountRepository.h>
#include <infrastructure/persistence/MySqlCharacterRepository.h>
#include <infrastructure/persistence/MySqlCreatureClassLevelStatsRepository.h>
#include <infrastructure/persistence/MySqlCreatureSpawnRepository.h>
#include <infrastructure/persistence/MySqlGmTicketRepository.h>
#include <infrastructure/persistence/MySqlGossipRepository.h>
#include <infrastructure/persistence/MySqlNpcTextRepository.h>
#include <infrastructure/persistence/MySqlNpcTemplateSearchRepository.h>
#include <infrastructure/persistence/MySqlPlayerCreateInfoRepository.h>
#include <infrastructure/persistence/MySqlRealmRepository.h>
#include <infrastructure/scripting/LuaGameScriptHost.h>
#include <infrastructure/world/DbCreatureSpawnBootstrap.h>
#include <infrastructure/world/MapCollisionQueriesStub.h>
#include <memory>
#include <mutex>
#include <shared/Banner.h>
#include <shared/Config.h>
#include <shared/Logger.h>
#include <shared/dbc/FactionTemplateDbc.h>
#include <shared/dbc/ItemDbHotfixStore.h>
#include <shared/dbc/LanguagesDbc.h>
#include <shared/dbc/SpellDifficultyDbc.h>
#include <thread>

namespace Firelands {

/// When `tui_runtime` is non-null, publishes services then returns (FTXUI owns
/// the main loop). When null, runs the plain headless / stdin-less loop here.
int RunWorldGameStack(std::shared_ptr<WorldFtxuiRuntime> tui_runtime,
                      std::atomic<bool> &stopRealmLink,
                      std::unique_ptr<std::thread> &realmLinkThread,
                      bool console_enabled) {
  Config &config = Config::Instance();

  if (config.GetNested<bool>({"RealmLink", "Enabled"}, false)) {
    realmLinkThread = std::make_unique<std::thread>([&config, &stopRealmLink] {
      RunRealmLinkOutbound(config, stopRealmLink);
    });
  }

  try {
    if (tui_runtime) {
      LOG_INFO("Starting World Server...");
    }
    std::string scriptsRoot = config.GetNested<std::string>(
        {"Scripting", "ScriptsDirectory"}, "scripts/lua");
    auto scriptHost = std::make_shared<LuaGameScriptHost>();
    if (!scriptHost->Init(scriptsRoot)) {
      LOG_WARN(
          "Lua script host failed to initialize (continuing without scripts)");
    } else {
      LOG_DEBUG("Lua script host ready, root: {}", scriptsRoot);
      scriptHost->FireEvent("world_startup", 0);
    }
    WorldService::Instance().SetScriptHost(std::move(scriptHost));

    const std::string collisionRoot =
        config.GetNested<std::string>({"Collision", "DataRoot"}, "");
    WorldService::Instance().SetCollisionQueries(
        std::make_shared<MapCollisionQueriesStub>(collisionRoot));

    std::string dbUser =
        config.GetNested<std::string>({"Database", "User"}, "firelands");
    std::string dbPass =
        config.GetNested<std::string>({"Database", "Password"}, "firelands");

    std::string authUrl = config.GetNested<std::string>(
        {"Database", "Auth", "URI"},
        "jdbc:mariadb://localhost:3306/firelands_auth");

    DatabaseMigrator::MigrateDirectory(authUrl, dbUser, dbPass, "sql");

    sql::Driver *driver = sql::mariadb::get_driver_instance();
    sql::Properties properties({{"user", dbUser}, {"password", dbPass}});

    std::shared_ptr<sql::Connection> authConn(
        driver->connect(authUrl, properties));

    std::string charUrl = config.GetNested<std::string>(
        {"Database", "Characters", "URI"},
        "jdbc:mariadb://localhost:3306/firelands_characters");

    std::shared_ptr<sql::Connection> charConn(
        driver->connect(charUrl, properties));

    std::string worldUrl = config.GetNested<std::string>(
        {"Database", "World", "URI"},
        "jdbc:mariadb://localhost:3306/firelands_world");
    std::shared_ptr<sql::Connection> worldConn(
        driver->connect(worldUrl, properties));

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
    const std::string charStartOutfitDbcPath =
        dbcBasePath + "/CharStartOutfit.dbc";
    auto playerCreateInfoService = std::make_shared<PlayerCreateInfoService>(
        playerCreateInfoRepo, charStartOutfitDbcPath, dbcBasePath);
    auto charService =
        std::make_shared<CharacterService>(charRepo, playerCreateInfoService);
    auto onlineCharRegistry =
        std::make_shared<OnlineCharacterSessionRegistry>();
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

    auto spellEntryStore = std::make_shared<SpellEntryDbcStore>();
    bool const spellDbcOk = spellEntryStore->Load(dbcBasePath + "/Spell.dbc");
    if (!spellDbcOk) {
      LOG_WARN("Spell.dbc not loaded from {}; definitions come only from "
               "`spell_dbc` if present.",
               dbcBasePath + "/Spell.dbc");
    }

    std::shared_ptr<ISpellCastTables const> spellCastTables;
    {
      auto tables = std::make_shared<SpellCastTablesDbc>();
      if (!tables->Load(dbcBasePath + "/SpellCastTimes.dbc",
                        dbcBasePath + "/SpellRange.dbc",
                        dbcBasePath + "/SpellCooldowns.dbc",
                        dbcBasePath + "/SpellPower.dbc",
                        dbcBasePath + "/SpellCategories.dbc")) {
        LOG_WARN("One or more spell DBCs were not loadable from {} (cast "
                 "times, range, cooldowns, "
                 "power, categories); some lookups use defaults.",
                 dbcBasePath);
      }
      spellCastTables = std::move(tables);
    }

    spellEntryStore->MergeSpellDbcRows(worldConn);
    spellEntryStore->MergeImmediateHealthFromSpellEffect(dbcBasePath +
                                                         "/SpellEffect.dbc");
    spellEntryStore->ApplySpellPowerManaFromTables(*spellCastTables);

    std::shared_ptr<ISpellDefinitionStore const> spellDefinitions;
    if (spellDbcOk || spellEntryStore->DefinitionCount() > 0u)
      spellDefinitions = spellEntryStore;

    SpellDifficultyDbc spellDifficultyDbc;
    if (spellDifficultyDbc.Load(dbcBasePath + "/SpellDifficulty.dbc")) {
      LOG_DEBUG("SpellDifficulty.dbc ready ({} difficulty rows).",
                spellDifficultyDbc.GetRowCount());
    } else {
      LOG_DEBUG("SpellDifficulty.dbc not loaded (optional; pairs with "
                "`spelldifficulty_dbc` when present).");
    }

    auto itemDbHotfix = std::make_shared<ItemDbHotfixStore>();
    itemDbHotfix->load(dbcBasePath);

    auto spellManager =
        std::make_shared<SpellManager>(spellDefinitions, spellCastTables);

    std::shared_ptr<FactionTemplateDbc> factionTemplateDbcOwned =
        std::make_shared<FactionTemplateDbc>();
    if (!factionTemplateDbcOwned->Load(dbcBasePath + "/FactionTemplate.dbc")) {
      LOG_WARN("FactionTemplate.dbc not loaded from {}; faction ids are not "
               "server-validated (place client DBCs under Data.DbcPath).",
               dbcBasePath + "/FactionTemplate.dbc");
      factionTemplateDbcOwned.reset();
    }
    std::shared_ptr<FactionTemplateDbc const> factionTemplateDbc =
        factionTemplateDbcOwned;

    if (auto script = WorldService::Instance().GetScriptHost())
      script->AttachFactionTemplateDbc(factionTemplateDbc);

    auto npcTemplateSearchRepo =
        std::make_shared<MySqlNpcTemplateSearchRepository>(worldConn);
    auto gossipRepo =
        std::make_shared<MySqlGossipRepository>(worldConn);
    auto npcTextRepo =
        std::make_shared<MySqlNpcTextRepository>(worldConn);
    auto creatureStatsRepo =
        std::make_shared<MySqlCreatureClassLevelStatsRepository>(worldConn);
    auto creatureSpawnRepo =
        std::make_shared<MySqlCreatureSpawnRepository>(worldConn);
    LoadDatabaseCreatureSpawns(*creatureSpawnRepo, *creatureStatsRepo,
                               factionTemplateDbc.get());

    auto sessionFactory =
        [authService, charService, commandService, accountDataRepo,
         languagesDbc, spellDefinitions, realmRepo, onlineCharRegistry,
         gmTicketService, itemDbHotfix, spellManager, npcTemplateSearchRepo,
         factionTemplateDbc, gossipRepo,
         npcTextRepo](boost::asio::ip::tcp::socket socket) {
          std::make_shared<WorldSession>(
              std::move(socket), authService, charService, commandService,
              accountDataRepo, languagesDbc, spellDefinitions, realmRepo,
              onlineCharRegistry, gmTicketService, itemDbHotfix, spellManager,
              npcTemplateSearchRepo, factionTemplateDbc, gossipRepo, npcTextRepo)
              ->Start();
        };

    auto worldServer = std::make_shared<AsyncNetworkServer>(sessionFactory);

    std::string bindIp = Config::Instance().GetNested<std::string>(
        {"Network", "BindAddress"}, "0.0.0.0");
    int port = Config::Instance().GetNested<int>({"Network", "Port"}, 8085);

    if (!worldServer->Start(bindIp, port)) {
      LOG_CRITICAL("Failed to start World Server.");
      return 1;
    }
    LOG_INFO("World Server listening on {}:{}", bindIp, port);

    auto interactiveConsole =
        std::make_shared<WorldInteractiveConsole>(commandService);
    commandService->SetShutdownRequestHandler(
        [interactiveConsole]() { interactiveConsole->RequestShutdown(); });

    if (tui_runtime) {
      interactiveConsole->Start(console_enabled, false);
      LOG_DEBUG("Terminal UI (FTXUI): logs above, command input fixed below.");
      {
        std::lock_guard<std::mutex> lock(tui_runtime->mutex);
        tui_runtime->world_server = worldServer;
        tui_runtime->interactive_console = interactiveConsole;
        tui_runtime->command_service = commandService;
        tui_runtime->services_ready = true;
      }
      return 0;
    }

    if (console_enabled) {
      interactiveConsole->Start(console_enabled, true);
      LOG_DEBUG("Interactive console (stdin); type .help or quit to exit.");
      while (!interactiveConsole->ShutdownRequested()) {
        worldServer->Update();
        interactiveConsole->ProcessPending();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    } else {
      while (!interactiveConsole->ShutdownRequested()) {
        worldServer->Update();
        commandService->PollScheduledRestart();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }

    stopRealmLink.store(true);
    LOG_INFO("World server main loop stopped.");
    LOG_INFO("World server shutting down...");
    WorldService::Instance().ResetForShutdown();
    worldServer->Stop();
    if (realmLinkThread && realmLinkThread->joinable()) {
      realmLinkThread->join();
    }
    commandService->SetShutdownRequestHandler({});
    return 0;

  } catch (std::exception &e) {
    LOG_CRITICAL("Fatal error: {}", e.what());
    return 1;
  }
}

int RunWorldApplication(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Config &config = Config::Instance();

  const bool stickyYaml = config.GetNestedBool({"Log", "StickyBanner"}, false);
  const bool stickyWant = ResolveStickyBanner(stickyYaml);
  if (stickyWant && !StdoutIsInteractiveTerminal()) {
    LOG_WARN(
        "Log.StickyBanner is enabled but stdout is not a TTY (pipe/redirect); "
        "using normal console layout.");
  }
  bool consoleEnabledForBanner =
      config.GetNested<bool>({"Console", "Enabled"}, true);
  if (consoleEnabledForBanner && !StdoutIsInteractiveTerminal()) {
    consoleEnabledForBanner = false;
  }
  const bool useTerminalUiForBanner = consoleEnabledForBanner;
  if (!useTerminalUiForBanner) {
    PrintBanner(BannerType::World, stickyWant);
  }

  Logger::Shutdown();
  Logger::Init(
      LoggerBuilder()
          .WithName("firelands-world")
          .WithConsole(true)
          .WithConsoleLevel(
              config.GetNested<LogLevel>({"Log", "Level"}, LogLevel::Info))
          .WithFile(true, config.GetNested<std::string>(
                              {"Log", "File"}, "logs/firelands-world.log"))
          .WithFileLevel(LogLevel::Debug)
          .WithRotatingFile(10 * 1024 * 1024, 5)
          .Build());

  bool console_enabled = config.GetNested<bool>({"Console", "Enabled"}, true);
  if (console_enabled && !StdoutIsInteractiveTerminal()) {
    LOG_DEBUG("Console.Enabled is true but stdout is not a TTY; interactive "
              "console disabled.");
    console_enabled = false;
  }

  const bool use_terminal_ui = console_enabled && StdoutIsInteractiveTerminal();

  std::atomic<bool> stopRealmLink{false};
  std::unique_ptr<std::thread> realmLinkThread;

  if (use_terminal_ui) {
    auto rt = std::make_shared<WorldFtxuiRuntime>();
    RunWorldFtxuiConsole(rt, [&](std::shared_ptr<WorldFtxuiRuntime> rt_in) {
      try {
        int const rc = RunWorldGameStack(rt_in, stopRealmLink, realmLinkThread,
                                         console_enabled);
        if (rc != 0) {
          std::lock_guard<std::mutex> lock(rt_in->mutex);
          rt_in->bootstrap_failed = true;
        }
      } catch (std::exception const &e) {
        LOG_CRITICAL("Fatal error: {}", e.what());
        std::lock_guard<std::mutex> lock(rt_in->mutex);
        rt_in->bootstrap_failed = true;
      }
    });

    stopRealmLink.store(true);

    bool failed = false;
    std::shared_ptr<AsyncNetworkServer> ws;
    std::shared_ptr<CommandService> cs;
    {
      std::lock_guard<std::mutex> lock(rt->mutex);
      failed = rt->bootstrap_failed;
      ws = rt->world_server;
      cs = rt->command_service;
    }
    if (failed) {
      if (ws) {
        WorldService::Instance().ResetForShutdown();
        ws->Stop();
      }
      if (cs) {
        cs->SetShutdownRequestHandler({});
      }
      if (realmLinkThread && realmLinkThread->joinable()) {
        realmLinkThread->join();
      }
      Logger::Shutdown();
      return 1;
    }

    if (ws) {
      LOG_INFO("World server main loop stopped.");
      LOG_INFO("World server shutting down...");
      WorldService::Instance().ResetForShutdown();
      ws->Stop();
      if (cs) {
        cs->SetShutdownRequestHandler({});
      }
    }
    if (realmLinkThread && realmLinkThread->joinable()) {
      realmLinkThread->join();
    }
    Logger::Shutdown();
    return 0;
  }

  LOG_INFO("Starting World Server...");
  int const rc = RunWorldGameStack(nullptr, stopRealmLink, realmLinkThread,
                                   console_enabled);
  Logger::Shutdown();
  return rc;
}

} // namespace Firelands