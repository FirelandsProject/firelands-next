#include <application/services/SRPService.h>
#include <infrastructure/persistence/DatabaseService.h>
#include <infrastructure/persistence/MySqlAccountRepository.h>
#include <infrastructure/persistence/MySqlRealmRepository.h>
#include <shared/Banner.h>
#include <shared/Config.h>
#include <infrastructure/persistence/MySqlRbacRepository.h>
#include <shared/game/AccessLevel.h>
#include <shared/game/RbacBuiltinRoles.h>
#include <shared/Logger.h>
#include <cctype>
#include <string>
#include <vector>

using namespace Firelands;

namespace {

/// True if @p s is non-empty and every character is a decimal digit (realm id
/// detection for `realm` argv disambiguation).
bool IsAsciiUnsignedInteger(char const *s) {
  if (!s || *s == '\0')
    return false;
  for (char const *p = s; *p; ++p) {
    if (!std::isdigit(static_cast<unsigned char>(*p)))
      return false;
  }
  return true;
}

} // namespace

void PrintUsage(const char *progName) {
  LOG_ERROR("Usage:");
  LOG_ERROR("  {} account <username> <password> [email] [expansion (0-3)] "
            "[staff_role moderator|gamemaster|administrator]",
            progName);
  LOG_ERROR("  {} realm <id> <name> <address> <port> [icon] [timezone] "
            "[secLevel] [population]",
            progName);
  LOG_ERROR("       (or <name> <id> <address> <port> ... if <name> is not "
            "all digits)");
}

int CreateAccount(int argc, char **argv,
                  std::shared_ptr<sql::Connection> &conn) {
  std::string user = argv[2];
  std::string pass = argv[3];
  std::string email = (argc >= 5) ? argv[4] : (user + "@firelands.com");
  uint8 expansion = (argc >= 6) ? static_cast<uint8>(std::stoi(argv[5])) : 3;

  if (expansion > 3) {
    LOG_ERROR("Invalid expansion level: {}. Maximum allowed is 3 (Cataclysm).",
              (int)expansion);
    Logger::Shutdown();
    return 1;
  }

  std::string staffRole;
  if (argc >= 7)
    staffRole = argv[6];

  auto accountRepo = std::make_shared<MySqlAccountRepository>(conn);
  auto rbacRepo = std::make_shared<MySqlRbacRepository>(conn);

  if (accountRepo->FindByUsername(user)) {
    LOG_WARN("Account '{}' already exists. Overwriting...", user);
    accountRepo->DeleteByUsername(user);
  }

  SRPData srpData = SRPService::GenerateVerifier(user, pass);
  Account acc;
  acc.username = user;
  acc.email = email;
  acc.salt = srpData.salt;
  acc.verifier = srpData.verifier;
  acc.expansion = expansion;
  acc.accessLevel = AccessLevel::Player;

  LOG_INFO("Inserting account into database...");
  accountRepo->Create(acc);
  if (!staffRole.empty()) {
    auto created = accountRepo->FindByUsername(user);
    if (created && !rbacRepo->SetPrimaryStaffRole(created->id, staffRole)) {
      LOG_WARN("Account created but staff role '{}' was not applied.", staffRole);
    } else if (created) {
      LOG_INFO("Staff role '{}' assigned.", staffRole);
    }
  }
  LOG_INFO("Account created successfully.");
  return 0;
}

int CreateRealm(int argc, char **argv, std::shared_ptr<sql::Connection> &conn) {
  if (argc < 6) {
    LOG_ERROR("realm requires <id> <name> <address> <port> (or <name> <id> "
              "<address> <port> when the name is not all digits) and optional "
              "[icon] [timezone] [secLevel] [population]");
    Logger::Shutdown();
    return 1;
  }
  std::string name;
  uint32_t id;
  if (IsAsciiUnsignedInteger(argv[2])) {
    id = std::stoul(argv[2]);
    name = argv[3];
  } else {
    name = argv[2];
    id = std::stoul(argv[3]);
  }
  std::string address = argv[4];
  uint16_t port = static_cast<uint16_t>(std::stoul(argv[5]));
  uint8_t icon = (argc >= 7) ? static_cast<uint8_t>(std::stoul(argv[6])) : 0;
  uint8_t timezone =
      (argc >= 8) ? static_cast<uint8_t>(std::stoul(argv[7])) : 1;
  uint8_t secLevel =
      (argc >= 9) ? static_cast<uint8_t>(std::stoul(argv[8])) : 0;
  float population = (argc >= 10) ? std::stof(argv[9]) : 0.0f;

  auto realmRepo = std::make_shared<MySqlRealmRepository>(conn);

  if (realmRepo->FindById(id)) {
    LOG_WARN("Realm ID '{}' already exists. Overwriting...", id);
    realmRepo->DeleteById(id);
  }

  Realm realm(id, name, address, port, icon, timezone, secLevel, population);
  LOG_INFO("Inserting realm id={} name='{}' {}:{}...", id, name, address,
           static_cast<int>(port));
  realmRepo->Create(realm);
  LOG_INFO("Realm created successfully.");
  return 0;
}

int main(int argc, char **argv) {
  Logger::Init(LoggerBuilder()
                   .WithName("dev-tools")
                   .WithConsole(true)
                   .WithConsoleLevel(LogLevel::Info)
                   .Build());

  PrintBanner(BannerType::Tools);
  LOG_INFO("--- Firelands DevTools ---");

  if (argc < 2) {
    PrintUsage(argv[0]);
    Logger::Shutdown();
    return 1;
  }

  auto config = Config::Instance();
  if (!config.Load("authserver.yaml")) {
    LOG_WARN("Could not load authserver.yaml, using defaults...");
  }

  std::string command = argv[1];

  try {
    std::string user =
        config.GetNested<std::string>({"Database", "User"}, "firelands");
    std::string pass =
        config.GetNested<std::string>({"Database", "Password"}, "firelands");

    std::string authUrl = config.GetNested<std::string>(
        {"Database", "Auth", "URI"},
        "jdbc:mariadb://localhost:3306/firelands_auth");

    LOG_INFO("Connecting to database...");
    DatabaseService dbService(authUrl, user, pass);
    auto conn = dbService.CreateConnection();

    // Call create account or create realm based on the command
    if (command == "account") {
      return CreateAccount(argc, argv, conn);
    } else if (command == "realm") {
      // Call create realm based on the command
      return CreateRealm(argc, argv, conn);
    } else {
      LOG_ERROR("Unknown command: {}", command);
      PrintUsage(argv[0]);
    }

  } catch (std::exception &e) {
    LOG_CRITICAL("Error: {}", e.what());
    Logger::Shutdown();
    return 1;
  }

  Logger::Shutdown();
  return 0;
}
