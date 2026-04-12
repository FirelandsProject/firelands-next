#include <vector>
#include <string>
#include <application/services/SRPService.h>
#include <infrastructure/persistence/MySqlAccountRepository.h>
#include <infrastructure/persistence/DatabaseService.h>
#include <shared/Banner.h>
#include <shared/Logger.h>

using namespace Firelands;

int main(int argc, char** argv) {
    Logger::Init(
        LoggerBuilder()
            .WithName("account-gen")
            .WithConsole(true)
            .WithConsoleLevel(LogLevel::Info)
            .Build()
    );

    PrintBanner();
    LOG_INFO("--- Firelands Account Generator & Inserter ---");

    if (argc < 3) {
        LOG_ERROR("Usage: {} <username> <password> [email] [expansion (0-4)]", argv[0]);
        LOG_ERROR("Example: {} admin admin123 admin@firelands.com 4", argv[0]);
        Logger::Shutdown();
        return 1;
    }

    std::string user = argv[1];
    std::string pass = argv[2];
    std::string email = (argc >= 4) ? argv[3] : (user + "@firelands.com");
    uint8 expansion = (argc >= 5) ? static_cast<uint8>(std::stoi(argv[4])) : 4;

    try {
        // 1. Generate SRP Verifier
        LOG_INFO("Generating SRP-6a credentials for user: {}...", user);
        SRPData srpData = SRPService::GenerateVerifier(user, pass);

        // 2. Database Connection
        LOG_INFO("Connecting to database...");
        
        // These credentials match src/main.cpp
        DatabaseService dbService("jdbc:mariadb://127.0.0.1:3306/firelands_auth", "firelands", "firelands");
        auto conn = dbService.CreateConnection();
        
        auto accountRepo = std::make_shared<MySqlAccountRepository>(conn);

        // 3. Delete if account already exists
        if (accountRepo->FindByUsername(user)) {
            LOG_WARN("Account '{}' already exists. Overwriting...", user);
            accountRepo->DeleteByUsername(user);
        }

        // 4. Create Account
        Account acc;
        acc.username = user;
        acc.email = email;
        acc.salt = srpData.salt;
        acc.verifier = srpData.verifier;
        acc.expansion = expansion;

        LOG_INFO("Inserting account into database...");
        accountRepo->Create(acc);

        LOG_INFO("Account created successfully.");
        LOG_INFO("Username:  {}", user);
        LOG_INFO("Email:     {}", email);
        LOG_INFO("Expansion: {}", static_cast<int>(expansion));

    } catch (sql::SQLException& e) {
        LOG_CRITICAL("Database error: {}", e.what());
        LOG_ERROR("Make sure the MariaDB container is running and accessible.");
        Logger::Shutdown();
        return 1;
    } catch (std::exception& e) {
        LOG_CRITICAL("Fatal error: {}", e.what());
        Logger::Shutdown();
        return 1;
    }

    Logger::Shutdown();
    return 0;
}
