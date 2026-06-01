#include "AuthApplication.h"

#include "AuthFtxuiConsole.h"
#include <application/services/AuthService.h>
#include <application/services/RealmListService.h>
#include <application/services/WebSessionService.h>
#include <boost/asio.hpp>
#include <chrono>
#include <conncpp.hpp>
#include <cstdlib>
#include <infrastructure/network/asio/AsyncNetworkServer.h>
#include <infrastructure/network/realm_link/RealmLinkSession.h>
#include <infrastructure/network/realm_link/RealmLiveRegistry.h>
#include <infrastructure/network/rest/RestAuthServer.h>
#include <infrastructure/network/sessions/AuthSession.h>
#include <infrastructure/persistence/DatabaseMigrator.h>
#include <infrastructure/persistence/MemoryWebSessionRepository.h>
#include <infrastructure/persistence/MySqlAccountRepository.h>
#include <infrastructure/persistence/MySqlRbacRepository.h>
#include <infrastructure/persistence/MySqlRealmRepository.h>
#include <memory>
#include <mutex>
#include <shared/Banner.h>
#include <shared/Config.h>
#include <shared/Logger.h>
#include <thread>

namespace Firelands {

using tcp = boost::asio::ip::tcp;

[[noreturn]] void
AuthRunPlainServerLoop(std::shared_ptr<AsyncNetworkServer> authServer,
                       std::shared_ptr<AsyncNetworkServer> realmLinkServer) {
  while (true) {
    authServer->Update();
    if (realmLinkServer) {
      realmLinkServer->Update();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

/// When `tui_runtime` is set, publishes running servers and returns. Otherwise
/// does not return (plain TCP loop).
int AuthRunGameStack(std::shared_ptr<AuthFtxuiRuntime> tui_runtime) {
  Config &config = Config::Instance();

  try {
    if (tui_runtime) {
      LOG_INFO("Starting Authentication Server...");
    }

    std::string dbUser =
        config.GetNested<std::string>({"Database", "User"}, "firelands");
    std::string dbPass =
        config.GetNested<std::string>({"Database", "Password"}, "firelands");

    std::string authUrl = config.GetNested<std::string>(
        {"Database", "Auth", "URI"},
        "jdbc:mariadb://localhost:3306/firelands_auth");

    DatabaseMigrator::MigrateAuthServerStartup(authUrl, dbUser, dbPass, "sql");

    sql::Driver *driver = sql::mariadb::get_driver_instance();
    sql::Properties properties({{"user", dbUser}, {"password", dbPass}});

    std::shared_ptr<sql::Connection> conn(driver->connect(authUrl, properties));
    LOG_DEBUG("Database connection established.");

    auto accountRepo = std::make_shared<MySqlAccountRepository>(conn);
    auto rbacRepo = std::make_shared<MySqlRbacRepository>(conn);
    auto realmRepo = std::make_shared<MySqlRealmRepository>(conn);

    auto authService = std::make_shared<AuthService>(accountRepo);

    std::string realmLinkToken =
        config.GetNestedScalarString({"RealmLink", "Token"}, "");
    if (realmLinkToken.empty())
      realmLinkToken = config.GetNestedScalarString({"RealmLink", "token"}, "");
    int const realmLinkPort =
        config.GetNested<int>({"RealmLink", "Port"}, 3725);
    std::string const realmLinkBind =
        config.GetNestedScalarString({"RealmLink", "BindAddress"}, "127.0.0.1");

    std::shared_ptr<RealmLiveRegistry> realmLive;
    if (!realmLinkToken.empty() && realmLinkPort > 0) {
      realmLive = std::make_shared<RealmLiveRegistry>();
      LOG_DEBUG("Realm-link: listener will bind {}:{} (realm list uses live "
                "state)",
                realmLinkBind, realmLinkPort);
    } else {
      bool const hasRl = config.HasNestedKey({"RealmLink"});
      bool const hasTok = config.HasNestedKey({"RealmLink", "Token"});
      char const *envCfg = std::getenv("FIRELANDS_AUTH_CONFIG");
      LOG_WARN(
          "Realm-link disabled: need non-empty RealmLink.Token and Port>0 "
          "(token_len={}, port={}). Loaded config: \"{}\" "
          "has_RealmLink={} has_RealmLink.Token={} FIRELANDS_AUTH_CONFIG={}",
          realmLinkToken.size(), realmLinkPort, config.GetLoadedConfigPath(),
          hasRl, hasTok, envCfg ? envCfg : "(unset)");
      if (envCfg && envCfg[0] != '\0') {
        LOG_WARN("FIRELANDS_AUTH_CONFIG is set; that file must include "
                 "RealmLink.Token or unset the variable.");
      }
    }

    auto realmService =
        std::make_shared<RealmListService>(realmRepo, realmLive);

    auto webSessionRepo = std::make_shared<MemoryWebSessionRepository>();
    auto webSessionService =
        std::make_shared<WebSessionService>(webSessionRepo);

    auto sessionFactory = [authService, realmService,
                           rbacRepo](boost::asio::ip::tcp::socket socket) {
      std::make_shared<AuthSession>(std::move(socket), authService, realmService,
                                    rbacRepo)
          ->Start();
    };
    auto authServer = std::make_shared<AsyncNetworkServer>(sessionFactory);

    std::shared_ptr<AsyncNetworkServer> realmLinkServer;
    if (realmLive) {
      realmLinkServer = std::make_shared<AsyncNetworkServer>(
          [realmLive, realmLinkToken](tcp::socket socket) {
            std::make_shared<RealmLinkSession>(std::move(socket), realmLive,
                                               realmLinkToken)
                ->Start();
          });
    }

    std::string bindIp =
        config.GetNested<std::string>({"Network", "BindAddress"}, "0.0.0.0");
    int netPort = config.GetNested<int>({"Network", "Port"}, 3724);

    if (!authServer->Start(bindIp, netPort)) {
      LOG_CRITICAL("Failed to start Authentication Server.");
      return 1;
    }

    LOG_INFO("Authentication Server listening on {}:{}", bindIp, netPort);

    if (realmLinkServer) {
      if (!realmLinkServer->Start(realmLinkBind,
                                  static_cast<uint16_t>(realmLinkPort))) {
        LOG_CRITICAL("Failed to start realm-link server on {}:{}.",
                     realmLinkBind, realmLinkPort);
        return 1;
      }
      LOG_INFO("Realm-link server listening on {}:{}", realmLinkBind,
               realmLinkPort);
    }

    std::string restBindIp =
        config.GetNested<std::string>({"Network", "BindAddress"}, "0.0.0.0");
    int restPort = config.GetNested<int>({"Network", "RestPort"}, 8081);

    auto restServer = std::make_shared<RestAuthServer>(
        authService, webSessionService, restBindIp,
        static_cast<uint16_t>(restPort));

    if (restServer->Start()) {
      LOG_DEBUG("REST Authentication API listening on {}:{}", bindIp, restPort);
    }

    if (tui_runtime) {
      LOG_DEBUG("Terminal UI (FTXUI): logs only; press Q or Ctrl+C to stop.");
      {
        std::lock_guard<std::mutex> lock(tui_runtime->mutex);
        tui_runtime->auth_server = authServer;
        tui_runtime->realm_link_server = realmLinkServer;
        tui_runtime->rest_server = restServer;
        tui_runtime->services_ready = true;
      }
      return 0;
    }

    AuthRunPlainServerLoop(authServer, realmLinkServer);

  } catch (sql::SQLException &e) {
    LOG_CRITICAL("Database error: {}", e.what());
    LOG_ERROR(
        "Please ensure Docker is running and the database is initialized.");
    return 1;
  } catch (std::exception &e) {
    LOG_CRITICAL("Fatal error: {}", e.what());
    return 1;
  }
}

int RunAuthApplication(int argc, char **argv) {
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
    PrintBanner(BannerType::Auth, stickyWant);
  }

  LogLevel consoleLevel =
      config.GetNested<LogLevel>({"Log", "Level"}, LogLevel::Info);
  std::string logFile =
      config.GetNested<std::string>({"Log", "File"}, "logs/firelands-auth.log");

  Logger::Shutdown();
  Logger::Init(LoggerBuilder()
                   .WithName("firelands-auth")
                   .WithConsole(true)
                   .WithConsoleLevel(consoleLevel)
                   .WithFile(true, logFile)
                   .WithFileLevel(LogLevel::Debug)
                   .WithRotatingFile(10 * 1024 * 1024, 5)
                   .Build());

  bool console_enabled = config.GetNested<bool>({"Console", "Enabled"}, true);
  if (console_enabled && !StdoutIsInteractiveTerminal()) {
    LOG_DEBUG("Console.Enabled is true but stdout is not a TTY; using plain "
              "log loop (no TUI).");
    console_enabled = false;
  }

  const bool use_terminal_ui = console_enabled && StdoutIsInteractiveTerminal();

  if (use_terminal_ui) {
    auto rt = std::make_shared<AuthFtxuiRuntime>();
    RunAuthFtxuiConsole(rt, [&](std::shared_ptr<AuthFtxuiRuntime> rt_in) {
      try {
        int const rc = AuthRunGameStack(rt_in);
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

    std::shared_ptr<RestAuthServer> rest;
    std::shared_ptr<AsyncNetworkServer> auth_srv;
    std::shared_ptr<AsyncNetworkServer> realm_srv;
    bool failed = false;
    {
      std::lock_guard<std::mutex> lock(rt->mutex);
      failed = rt->bootstrap_failed;
      rest = rt->rest_server;
      auth_srv = rt->auth_server;
      realm_srv = rt->realm_link_server;
    }

    if (rest) {
      rest->Stop();
    }
    if (auth_srv) {
      auth_srv->Stop();
    }
    if (realm_srv) {
      realm_srv->Stop();
    }

    if (failed) {
      Logger::Shutdown();
      return 1;
    }

    LOG_INFO("Authentication server stopped.");
    Logger::Shutdown();
    return 0;
  }

  LOG_INFO("Starting Authentication Server...");
  int const rc = AuthRunGameStack(nullptr);
  Logger::Shutdown();
  return rc;
}

} // namespace Firelands