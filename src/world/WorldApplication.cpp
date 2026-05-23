#include "WorldApplication.h"

#include "WorldFtxuiConsole.h"
#include "WorldInteractiveConsole.h"
#include <infrastructure/combat/InMemoryThreatManager.h>
#include <infrastructure/combat/MySqlSpellProcessor.h>
#include <application/combat/CombatService.h>
#include <domain/combat/CombatEngine.h>
#include <application/services/GmTicketService.h>
#include <application/services/OnlineCharacterSessionRegistry.h>
#include <application/services/PlayerCreateInfoService.h>
#include <application/services/CommandService.h>
#include <application/services/WorldService.h>
#include <application/services/WorldRatesConfig.h>
#include <application/spell/SpellManager.h>
#include <infrastructure/world/MapAuraTicker.h>
#include <infrastructure/world/MapPlayerRegenTicker.h>
#include <atomic>
#include <chrono>
#include <conncpp.hpp>
#include <domain/repositories/ISpellCastTables.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <infrastructure/dbc/SpellCastTablesDbc.h>
#include <infrastructure/dbc/SpellEntryDbcStore.h>
#include <shared/dbc/SpellVisualDbc.h>
#include <infrastructure/network/asio/AsyncNetworkServer.h>
#include <infrastructure/network/realm_link/RealmLinkOutbound.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/persistence/DatabaseMigrator.h>
#include <infrastructure/persistence/MySqlAccountDataRepository.h>
#include <infrastructure/persistence/MySqlAccountRepository.h>
#include <infrastructure/persistence/MySqlCharacterRepository.h>
#include <infrastructure/persistence/MySqlCreatureClassLevelStatsRepository.h>
#include <infrastructure/persistence/MySqlCreatureSpawnRepository.h>
#include <infrastructure/persistence/MySqlPhaseAreaCatalogRepository.h>
#include <infrastructure/persistence/MySqlPhaseConditionRepository.h>
#include <infrastructure/persistence/MySqlPhaseGroupCatalogRepository.h>
#include <infrastructure/persistence/MySqlPlayerQuestProgressRepository.h>
#include <application/world/PhaseAreaCatalog.h>
#include <application/world/PhaseAreaCatalogBuilder.h>
#include <application/world/PhaseGroupCatalog.h>
#include <application/world/WorldRuntimeAccess.h>
#include <infrastructure/persistence/MySqlGmTicketRepository.h>
#include <infrastructure/persistence/MySqlGossipRepository.h>
#include <infrastructure/persistence/MySqlNpcTextRepository.h>
#include <infrastructure/persistence/MySqlQuestGossipRepository.h>
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
#include <shared/system/SystemBeep.h>
#include <shared/dbc/AreaTableDbc.h>
#include <shared/dbc/FactionTemplateDbc.h>
#include <shared/game/SkillLineCategories.h>
#include <shared/dbc/ItemDbHotfixStore.h>
#include <shared/dbc/EmotesTextDbc.h>
#include <shared/dbc/LanguagesDbc.h>
#include <shared/dbc/NameGenDbc.h>
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

    ExperienceRates const xpRates = LoadExperienceRatesFromConfig(config);
    WorldService::Instance().SetExperienceRates(xpRates);
    LOG_INFO("Experience rates: Kill={} Quest={} Explore={}", xpRates.kill, xpRates.quest,
             xpRates.explore);

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
        const std::string dbcBasePath = Config::ResolveDataDirectory(
                config.GetNested<std::string>({"Data", "DbcPath"}, "data/dbc"));
    const std::string charStartOutfitDbcPath =
        dbcBasePath + "/CharStartOutfit.dbc";
    auto playerCreateInfoService = std::make_shared<PlayerCreateInfoService>(
        playerCreateInfoRepo, charStartOutfitDbcPath, dbcBasePath);
    auto nameGenDbc = std::make_shared<NameGenDbc>();
    if (!nameGenDbc->Load(dbcBasePath + "/NameGen.dbc")) {
      LOG_WARN("NameGen.dbc not loaded from {}; character randomize-name button "
               "will not work.",
               dbcBasePath + "/NameGen.dbc");
      nameGenDbc.reset();
    }
    auto charService = std::make_shared<CharacterService>(
        charRepo, playerCreateInfoService, nameGenDbc);
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

    auto emotesTextDbc = std::make_shared<EmotesTextDbc>();
    if (!emotesTextDbc->Load(dbcBasePath + "/EmotesText.dbc")) {
      LOG_WARN("EmotesText.dbc not loaded from {}; /wave and other text emotes "
               "will not play.",
               dbcBasePath + "/EmotesText.dbc");
      emotesTextDbc.reset();
    }

    auto spellEntryStore = std::make_shared<SpellEntryDbcStore>();
    if (!LoadSkillLineCategories(dbcBasePath + "/SkillLine.dbc")) {
      LOG_WARN("SkillLine.dbc not loaded; starter skill filter uses legacy IDs only.");
    }
    if (!LoadSkillLineAbilitySpellIndex(dbcBasePath + "/SkillLineAbility.dbc")) {
      LOG_WARN("SkillLineAbility.dbc not loaded; profession/guild spell filter may be incomplete.");
    }

    bool const spellDbcOk = spellEntryStore->Load(dbcBasePath + "/Spell.dbc");
    if (!spellDbcOk) {
            LOG_ERROR("Spell.dbc not loaded from {}; spell definitions lack SpellPower "
                                "ids — all casts will have zero cost. Fix Data.DbcPath or run "
                                "world from the repo root.",
               dbcBasePath + "/Spell.dbc");
    }

    std::shared_ptr<ISpellCastTables const> spellCastTables;
    {
      auto tables = std::make_shared<SpellCastTablesDbc>();
      if (!tables->Load(dbcBasePath + "/SpellCastTimes.dbc",
                        dbcBasePath + "/SpellRange.dbc",
                        dbcBasePath + "/SpellCooldowns.dbc",
                        dbcBasePath + "/SpellPower.dbc",
                        dbcBasePath + "/SpellCategories.dbc",
                        dbcBasePath + "/SpellDuration.dbc")) {
        LOG_WARN("One or more spell DBCs were not loadable from {} (cast "
                 "times, range, cooldowns, "
                 "power, categories, duration); timed auras may not expire.",
                 dbcBasePath);
    }
            if (!tables->HasSpellPowerRows()) {
                LOG_ERROR("SpellPower.dbc not loaded from {}; all spell costs resolve "
                                    "to zero. Resolved DbcPath={}",
                                    dbcBasePath + "/SpellPower.dbc", dbcBasePath);
    } else {
                LOG_INFO("SpellPower.dbc: {} cost rows (DbcPath={}).",
                                  tables->SpellPowerRowCount(), dbcBasePath);
    }
      spellCastTables = std::move(tables);
    }

        if (spellDbcOk) {
            if (auto fireball = spellEntryStore->GetDefinition(133u)) {
                uint32 const sampleCost = spellCastTables->ResolveSpellPowerCost(
                        fireball->spellPowerId, fireball->powerType, 10, 1, 1000u);
                LOG_INFO(
                        "Spell.dbc loaded ({} definitions). Fireball(133) spellPowerId={} "
                        "sampleCost@1000 mana={}",
                        spellEntryStore->DefinitionCount(), fireball->spellPowerId, sampleCost);
    }
    }
    WorldService::Instance().SetSpellCastTables(spellCastTables);

    spellEntryStore->MergeSpellDbcRows(worldConn);
    spellEntryStore->MergeImmediateHealthFromSpellEffect(dbcBasePath +
                                                         "/SpellEffect.dbc");
    spellEntryStore->ApplySpellPowerManaFromTables(*spellCastTables);
    if (!spellEntryStore->LoadSpellLevels(dbcBasePath + "/SpellLevels.dbc")) {
      LOG_WARN("SpellLevels.dbc not loaded from {}; spell level gates use "
               "permissive defaults.",
               dbcBasePath + "/SpellLevels.dbc");
    }

    std::shared_ptr<ISpellDefinitionStore const> spellDefinitions;
    if (spellDbcOk || spellEntryStore->DefinitionCount() > 0u)
      spellDefinitions = spellEntryStore;
    WorldService::Instance().SetSpellDefinitions(spellDefinitions);

    auto spellVisualDbc = std::make_shared<SpellVisualDbc>();
    if (!spellVisualDbc->Load(dbcBasePath + "/SpellVisual.dbc")) {
      LOG_WARN("SpellVisual.dbc not loaded from {}; spell impact VFX disabled.",
               dbcBasePath + "/SpellVisual.dbc");
      spellVisualDbc.reset();
    }
    if (spellVisualDbc)
      WorldService::Instance().SetSpellVisualDbc(spellVisualDbc);

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
    WorldService::Instance().SetFactionTemplateDbc(factionTemplateDbc);

    auto areaTableDbcOwned = std::make_shared<AreaTableDbc>();
    if (!areaTableDbcOwned->Load(dbcBasePath + "/AreaTable.dbc")) {
      LOG_WARN("AreaTable.dbc not loaded from {}; phase_area parent walk disabled.",
               dbcBasePath + "/AreaTable.dbc");
      areaTableDbcOwned.reset();
    }
    WorldService::Instance().SetAreaTableDbc(areaTableDbcOwned);

    auto npcTemplateSearchRepo =
        std::make_shared<MySqlNpcTemplateSearchRepository>(worldConn);
    auto gossipRepo =
        std::make_shared<MySqlGossipRepository>(worldConn);
    auto npcTextRepo =
        std::make_shared<MySqlNpcTextRepository>(worldConn);
    auto questGossipRepo =
        std::make_shared<MySqlQuestGossipRepository>(worldConn);
    auto creatureStatsRepo =
        std::make_shared<MySqlCreatureClassLevelStatsRepository>(worldConn);
    auto creatureSpawnRepo =
        std::make_shared<MySqlCreatureSpawnRepository>(worldConn);
    auto phaseGroupRepo = std::make_shared<MySqlPhaseGroupCatalogRepository>(worldConn);
    auto phaseAreaRepo = std::make_shared<MySqlPhaseAreaCatalogRepository>(worldConn);
    auto phaseConditionRepo = std::make_shared<MySqlPhaseConditionRepository>(worldConn);
    auto questProgressRepo = std::make_shared<MySqlPlayerQuestProgressRepository>(charConn);
    auto phaseGroupCatalogOwned = std::make_shared<PhaseGroupCatalog>();
    phaseGroupCatalogOwned->Load(phaseGroupRepo->LoadPhaseGroups());
    WorldService::Instance().SetPhaseGroupCatalog(phaseGroupCatalogOwned);
    auto phaseAreaCatalogOwned = std::make_shared<PhaseAreaCatalog>();
    phaseAreaCatalogOwned->Load(BuildPhaseAreaCatalogLoadMap(
        phaseAreaRepo->LoadAreaPhases(), phaseConditionRepo->LoadPhaseConditions()));
    WorldService::Instance().SetPhaseAreaCatalog(phaseAreaCatalogOwned);
    LoadDatabaseCreatureSpawns(*creatureSpawnRepo, *creatureStatsRepo,
                               *phaseGroupCatalogOwned, factionTemplateDbc.get());

    auto combatService = std::make_shared<application::CombatService>(
        std::make_shared<combat::CombatEngine>(
            std::make_shared<infrastructure::InMemoryThreatManager>(),
            std::make_shared<infrastructure::MySqlSpellProcessor>()));

    auto sessionFactory =
        [authService, charService, commandService, accountDataRepo,
         languagesDbc, spellDefinitions, realmRepo,
         onlineCharRegistry, gmTicketService, itemDbHotfix, spellManager,
         combatService, npcTemplateSearchRepo, factionTemplateDbc, gossipRepo, npcTextRepo,
         questGossipRepo, questProgressRepo, emotesTextDbc](boost::asio::ip::tcp::socket socket) {
          std::make_shared<WorldSession>(
              std::move(socket), authService, charService, commandService,
              accountDataRepo, languagesDbc, spellDefinitions, realmRepo,
              onlineCharRegistry, gmTicketService, itemDbHotfix, spellManager,
              combatService, npcTemplateSearchRepo, factionTemplateDbc, gossipRepo,
              npcTextRepo, questGossipRepo, questProgressRepo, emotesTextDbc, WorldRuntimePtr())
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

    if (config.GetNested<bool>({"RealmLink", "Enabled"}, false) && !realmLinkThread) {
      realmLinkThread = std::make_unique<std::thread>([&config, &stopRealmLink] {
        RunRealmLinkOutbound(config, stopRealmLink);
      });
    }

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
            LOG_INFO("World Server is ready and accepting connections.");
            PlaySystemBeep();
    return 0;
    }

    auto const auraTickInterval = std::chrono::milliseconds(100);
    auto lastAuraTick = std::chrono::steady_clock::now();

        LOG_INFO("World Server is ready and accepting connections.");
        PlaySystemBeep();

    if (console_enabled) {
      interactiveConsole->Start(console_enabled, true);
      LOG_DEBUG("Interactive console (stdin); type .help or quit to exit.");
      while (!interactiveConsole->ShutdownRequested()) {
        worldServer->Update();
        interactiveConsole->ProcessPending();
        auto const tickNow = std::chrono::steady_clock::now();
        if (tickNow - lastAuraTick >= auraTickInterval) {
          TickMapAuras(tickNow);
          TickMapPlayerResourceRegen(tickNow);
          lastAuraTick = tickNow;
    }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    } else {
      while (!interactiveConsole->ShutdownRequested()) {
        worldServer->Update();
        commandService->PollScheduledRestart();
        auto const tickNow = std::chrono::steady_clock::now();
        if (tickNow - lastAuraTick >= auraTickInterval) {
          TickMapAuras(tickNow);
          TickMapPlayerResourceRegen(tickNow);
          lastAuraTick = tickNow;
    }
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