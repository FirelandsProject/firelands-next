#include <infrastructure/persistence/DatabaseMigrator.h>

#include <conncpp.hpp>
#include <filesystem>
#include <fstream>
#include <infrastructure/persistence/SqlStatementSplitter.h>
#include <memory>
#include <shared/Logger.h>

namespace Firelands {

namespace {

void EnsureSchemaMigrationsTable(std::shared_ptr<sql::Connection> conn) {
  std::unique_ptr<sql::Statement> st(conn->createStatement());
  st->execute("CREATE DATABASE IF NOT EXISTS `firelands_auth`");
  st->execute(
      "CREATE TABLE IF NOT EXISTS `firelands_auth`.`schema_migrations` ("
      "`migration` VARCHAR(255) NOT NULL,"
      "`applied_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
      "PRIMARY KEY (`migration`)"
      ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
}

bool IsMigrationApplied(std::shared_ptr<sql::Connection> conn,
                        std::string const &migrationFilename) {
  try {
    std::unique_ptr<sql::PreparedStatement> ps(conn->prepareStatement(
        "SELECT COUNT(*) FROM `firelands_auth`.`schema_migrations` WHERE "
        "`migration` = ?"));
    ps->setString(1, migrationFilename);
    std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
    if (rs->next())
      return rs->getUInt(1) > 0;
  } catch (sql::SQLException &e) {
    LOG_ERROR("Could not read schema_migrations: {}", e.what());
  }
  return false;
}

void RecordMigrationApplied(std::shared_ptr<sql::Connection> conn,
                            std::string const &migrationFilename) {
  try {
    std::unique_ptr<sql::PreparedStatement> ps(conn->prepareStatement(
        "INSERT INTO `firelands_auth`.`schema_migrations` (`migration`) "
        "VALUES (?)"));
    ps->setString(1, migrationFilename);
    ps->execute();
  } catch (sql::SQLException &e) {
    LOG_WARN("Could not record migration {} (may already be recorded): {}",
             migrationFilename, e.what());
  }
}

bool IsDatabaseFresh(std::shared_ptr<sql::Connection> conn,
                     std::string const &database) {
  try {
    std::unique_ptr<sql::PreparedStatement> ps(conn->prepareStatement(
        "SELECT COUNT(*) FROM information_schema.SCHEMATA WHERE SCHEMA_NAME = "
        "?"));
    ps->setString(1, database);
    std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
    if (!rs->next() || rs->getUInt(1) == 0)
      return true;

    ps.reset(conn->prepareStatement(
        "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = ? "
        "AND table_type = 'BASE TABLE'"));
    ps->setString(1, database);
    rs.reset(ps->executeQuery());
    if (rs->next())
      return rs->getUInt(1) == 0;
  } catch (sql::SQLException &e) {
    LOG_WARN("Could not inspect database {} freshness: {}", database, e.what());
  }
  return false;
}

void ApplySqlFile(std::string const &uri, std::string const &user,
                  std::string const &password, std::filesystem::path const &path,
                  std::shared_ptr<sql::Connection> bookkeepingConn) {
  std::string const name = path.filename().string();
  if (IsMigrationApplied(bookkeepingConn, name)) {
    LOG_DEBUG("Skipping already-applied SQL: {}", name);
    return;
  }

  LOG_INFO("Applying SQL: {}", name);
  size_t const failed = DatabaseMigrator::Migrate(uri, user, password, path.string());
  if (failed == 0)
    RecordMigrationApplied(bookkeepingConn, name);
  else
    LOG_WARN(
        "SQL file {} had {} failed statement(s); not marking as applied "
        "(fix SQL or DB, then delete its row from "
        "firelands_auth.schema_migrations if you need a retry).",
        name, failed);
}

} // namespace

void DatabaseMigrator::MigrateAuthServerStartup(const std::string &authUri,
                                                const std::string &user,
                                                const std::string &password,
                                                const std::string &sqlDir) {
  MigrateForRole(MigrationServerRole::Auth, authUri, {}, {}, user, password,
                 sqlDir);
}

void DatabaseMigrator::MigrateWorldServerStartup(
    const std::string &bookkeepingAuthUri, const std::string &charactersUri,
    const std::string &worldUri, const std::string &user,
    const std::string &password, const std::string &sqlDir) {
  MigrateForRole(MigrationServerRole::World, bookkeepingAuthUri, charactersUri,
                 worldUri, user, password, sqlDir);
}

std::string DatabaseMigrator::UriForDatabaseInRole(
    std::string const &database, std::string const &serverUri,
    std::string const &authUri, std::string const &charactersUri,
    std::string const &worldUri) {
  if (database == kAuthDatabase)
    return authUri;
  if (database == kCharactersDatabase)
    return charactersUri.empty() ? JdbcUriForDatabase(serverUri, database)
                                 : charactersUri;
  if (database == kWorldDatabase)
    return worldUri.empty() ? JdbcUriForDatabase(serverUri, database) : worldUri;
  return JdbcUriForDatabase(serverUri, database);
}

void DatabaseMigrator::MigrateForRole(MigrationServerRole role,
                                      const std::string &bookkeepingAuthUri,
                                      const std::string &charactersUri,
                                      const std::string &worldUri,
                                      const std::string &user,
                                      const std::string &password,
                                      const std::string &sqlDir) {
  std::filesystem::path const sqlRoot(sqlDir);
  if (!std::filesystem::is_directory(sqlRoot)) {
    LOG_ERROR("Migration directory does not exist: {}", sqlDir);
    return;
  }

  char const *roleLabel = role == MigrationServerRole::Auth ? "auth" : "world";
  LOG_INFO("Running {} database migrations from {}", roleLabel, sqlDir);

  try {
    std::string const serverUri =
        ExtractJdbcServerUri(bookkeepingAuthUri.empty() ? charactersUri
                                                        : bookkeepingAuthUri);

    sql::Driver *driver = sql::mariadb::get_driver_instance();
    sql::Properties properties({{"user", user}, {"password", password}});

    std::shared_ptr<sql::Connection> bookkeepingConn(
        driver->connect(serverUri, properties));
    EnsureSchemaMigrationsTable(bookkeepingConn);

    for (std::string const &database : TargetDatabasesForRole(role)) {
      std::string const dbUri = UriForDatabaseInRole(
          database, serverUri, bookkeepingAuthUri, charactersUri, worldUri);

      bool const fresh = IsDatabaseFresh(bookkeepingConn, database);
      if (fresh) {
        LOG_INFO("Database `{}` is new — applying init schema", database);
        for (auto const &initPath : InitScriptPathsForDatabase(sqlRoot, database))
          ApplySqlFile(dbUri, user, password, initPath, bookkeepingConn);
      } else {
        LOG_DEBUG("Database `{}` exists — skipping init schema", database);
      }

      for (auto const &migrationPath :
           MigrationPathsForDatabase(sqlRoot, database))
        ApplySqlFile(dbUri, user, password, migrationPath, bookkeepingConn);
    }
  } catch (sql::SQLException &e) {
    LOG_ERROR("Migration bookkeeping error: {}", e.what());
  }

  LOG_DEBUG("{} database migrations finished.", roleLabel);
}

size_t DatabaseMigrator::Migrate(const std::string &uri, const std::string &user,
                                 const std::string &password,
                                 const std::string &sqlFilePath) {
  try {
    sql::Driver *driver = sql::mariadb::get_driver_instance();
    sql::Properties properties({{"user", user}, {"password", password}});

    std::shared_ptr<sql::Connection> conn(driver->connect(uri, properties));

    std::ifstream file(sqlFilePath);
    if (!file.is_open()) {
      LOG_ERROR("Could not open SQL schema file: {}", sqlFilePath);
      return 1;
    }

    LOG_DEBUG("Executing SQL file: {}", sqlFilePath);

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    size_t failures = 0;

    try {
      std::string::size_type dbPos =
          content.find("CREATE DATABASE IF NOT EXISTS `firelands_");
      if (dbPos != std::string::npos) {
        std::string::size_type backtickStart = content.find('`', dbPos);
        std::string::size_type backtickEnd = content.find('`', backtickStart + 1);
        if (backtickStart != std::string::npos &&
            backtickEnd != std::string::npos) {
          std::string targetDb =
              content.substr(backtickStart + 1, backtickEnd - backtickStart - 1);
          std::unique_ptr<sql::Statement> createStmt(conn->createStatement());
          createStmt->execute("CREATE DATABASE IF NOT EXISTS `" + targetDb + "`");
          conn->setSchema(targetDb);
          LOG_DEBUG("Created/selected database: {}", targetDb);
        }
      }

      std::string cleanedContent;
      std::istringstream iss(content);
      std::string line;
      while (std::getline(iss, line)) {
        if (line.find("--") != 0 && !line.empty()) {
          cleanedContent += line + "\n";
        } else if (line.find("--") == 0) {
          cleanedContent += line + "\n";
        }
      }

      std::vector<std::string> statements = SplitSqlStatements(cleanedContent);
      for (const auto &stmt : statements) {
        std::string trimmed = stmt;
        trimmed.erase(0, trimmed.find_first_not_of(" \n\r\t"));

        if (trimmed.empty())
          continue;

        if (trimmed.find("CREATE DATABASE") != std::string::npos)
          continue;

        if (trimmed.rfind("USE ", 0) == 0) {
          std::string db;
          if (auto const tick1 = trimmed.find('`'); tick1 != std::string::npos) {
            if (auto const tick2 = trimmed.find('`', tick1 + 1);
                tick2 != std::string::npos)
              db = trimmed.substr(tick1 + 1, tick2 - tick1 - 1);
          } else {
            std::string rest = trimmed.substr(4);
            rest.erase(0, rest.find_first_not_of(" \t\r\n"));
            if (auto const semi = rest.find(';'); semi != std::string::npos)
              rest = rest.substr(0, semi);
            if (auto const last = rest.find_last_not_of(" \t\r\n");
                last != std::string::npos)
              rest.erase(last + 1);
            db = rest;
          }
          if (!db.empty()) {
            conn->setSchema(db);
            LOG_DEBUG("Selected database: {}", db);
          }
          continue;
        }

        try {
          std::unique_ptr<sql::Statement> stmnt(conn->createStatement());
          stmnt->execute(stmt);
        } catch (sql::SQLException &e) {
          int code = e.getErrorCode();
          if (code == 1060 || code == 1061 || code == 1050) {
            LOG_DEBUG("Skipped (already exists): {}", trimmed.substr(0, 60));
          } else {
            LOG_WARN("Failed: {} - {}", trimmed.substr(0, 60), e.what());
            ++failures;
          }
        }
      }
    } catch (sql::SQLException &e) {
      LOG_ERROR("Migration error: {}", e.what());
      failures = 1;
    }

    LOG_DEBUG("SQL file completed: {} ({} failed statements)", sqlFilePath,
              failures);
    return failures;

  } catch (sql::SQLException &e) {
    LOG_ERROR("Migration error in {}: {}", sqlFilePath, e.what());
    return 1;
  } catch (std::exception &e) {
    LOG_ERROR("General error during migration of {}: {}", sqlFilePath, e.what());
    return 1;
  }
}

} // namespace Firelands
