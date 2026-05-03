#ifndef FIRELANDS_INFRASTRUCTURE_PERSISTENCE_DATABASE_MIGRATOR_H
#define FIRELANDS_INFRASTRUCTURE_PERSISTENCE_DATABASE_MIGRATOR_H
#include <algorithm>
#include <conncpp.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <shared/Logger.h>
#include <string>
#include <vector>

namespace Firelands {

/**
 * @brief Utility class to handle database schema validation and automatic
 * creation.
 */
class DatabaseMigrator {
public:
  /**
   * @brief Validates and executes all SQL files in a directory in alphabetical
   * order.
   */
  static void MigrateDirectory(const std::string &uri, const std::string &user,
                               const std::string &password,
                               const std::string &dirPath) {
    if (!std::filesystem::exists(dirPath)) {
      LOG_ERROR("Migration directory does not exist: {}", dirPath);
      return;
    }

    std::vector<std::string> sqlFiles;

    std::filesystem::path bundledPath = std::filesystem::path(dirPath) / "bundled";
    std::filesystem::path initPath = std::filesystem::path(dirPath) / "init";
    std::filesystem::path migrationsPath = std::filesystem::path(dirPath) / "migrations";

    auto appendSortedSql = [](std::vector<std::string> &out,
                              const std::filesystem::path &dir,
                              bool skipZzPrefix) {
      if (!std::filesystem::exists(dir))
        return;
      std::vector<std::string> chunk;
      for (const auto &entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".sql")
          continue;
        const std::string name = entry.path().filename().string();
        if (skipZzPrefix && name.size() >= 3 && name.compare(0, 3, "zz_") == 0)
          continue;
        chunk.push_back(entry.path().string());
      }
      std::sort(chunk.begin(), chunk.end());
      out.insert(out.end(), chunk.begin(), chunk.end());
    };

    appendSortedSql(sqlFiles, bundledPath, true);
    appendSortedSql(sqlFiles, initPath, false);
    appendSortedSql(sqlFiles, migrationsPath, false);

    LOG_DEBUG(
        "Starting database migrations from directory: {} (bundled, then init, "
        "then migrations; bundled zz_*.sql skipped)",
        dirPath);

    try {
      sql::Driver *driver = sql::mariadb::get_driver_instance();
      sql::Properties properties({{"user", user}, {"password", password}});

      std::string baseUri = uri;
      auto dbPos = uri.rfind("/firelands_");
      if (dbPos != std::string::npos) {
        baseUri = uri.substr(0, dbPos);
      }

      std::shared_ptr<sql::Connection> conn(driver->connect(baseUri, properties));
      EnsureSchemaMigrationsTable(conn);

      for (const auto &file : sqlFiles) {
        const std::string name =
            std::filesystem::path(file).filename().string();
        if (IsMigrationApplied(conn, name)) {
          LOG_DEBUG("Skipping already-applied migration: {}", name);
          continue;
        }
        const size_t failedStatements = Migrate(uri, user, password, file);
        if (failedStatements == 0)
          RecordMigrationApplied(conn, name);
        else
          LOG_WARN(
              "Migration {} had {} failed statement(s); not marking as applied "
              "(fix SQL or DB, then delete its row from "
              "firelands_auth.schema_migrations if you need a retry).",
              name, failedStatements);
      }
    } catch (sql::SQLException &e) {
      LOG_ERROR("Migration bookkeeping error: {}", e.what());
    }

    LOG_DEBUG("All migrations from {} completed.", dirPath);
  }

  /**
   * @brief Validates and executes an SQL schema file.
   *
   * @param host Database host
   * @param port Database port
   * @param user Database user
   * @param password Database password
   * @param sqlFilePath Path to the .sql file to execute
   */
  /// @return Number of statements that failed to execute (0 = full success).
  static size_t Migrate(const std::string &uri, const std::string &user,
                        const std::string &password,
                        const std::string &sqlFilePath) {
    try {
      sql::Driver *driver = sql::mariadb::get_driver_instance();

      // We connect without a specific database to ensure we can create it if it
      // doesn't exist The SQL script should contain "CREATE DATABASE IF NOT
      // EXISTS" and "USE" statements.
      sql::Properties properties({{"user", user}, {"password", password}});

      std::shared_ptr<sql::Connection> conn(driver->connect(uri, properties));

      std::ifstream file(sqlFilePath);
      if (!file.is_open()) {
        LOG_ERROR("Could not open SQL schema file: {}", sqlFilePath);
        return 1;
      }

      LOG_DEBUG("Validating database schema: {}", sqlFilePath);

std::string content((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());

      size_t failures = 0;

      try {
        std::string::size_type dbPos = content.find("CREATE DATABASE IF NOT EXISTS `firelands_");
        if (dbPos != std::string::npos) {
          std::string::size_type backtickStart = content.find('`', dbPos);
          std::string::size_type backtickEnd = content.find('`', backtickStart + 1);
          if (backtickStart != std::string::npos && backtickEnd != std::string::npos) {
            std::string targetDb = content.substr(backtickStart + 1, backtickEnd - backtickStart - 1);
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

        std::vector<std::string> statements = SplitStatements(cleanedContent);
        for (const auto &stmt : statements) {
          std::string trimmed = stmt;
          trimmed.erase(0, trimmed.find_first_not_of(" \n\r\t"));

          if (trimmed.empty())
            continue;

          if (trimmed.find("CREATE DATABASE") != std::string::npos)
            continue;

          if (trimmed.rfind("USE ", 0) == 0)
            continue;

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

      LOG_DEBUG("Schema validation completed for: {} ({} failed statements)", sqlFilePath, failures);
      return failures;

    } catch (sql::SQLException &e) {
      LOG_ERROR("Migration error in {}: {}", sqlFilePath, e.what());
      return 1;
    } catch (std::exception &e) {
      LOG_ERROR("General error during migration of {}: {}", sqlFilePath,
                e.what());
      return 1;
    }
  }

private:
  /// Ensures `firelands_auth.schema_migrations` exists (tracks applied .sql files).
  static void EnsureSchemaMigrationsTable(std::shared_ptr<sql::Connection> conn) {
    std::unique_ptr<sql::Statement> st(conn->createStatement());
    st->execute(
        "CREATE DATABASE IF NOT EXISTS `firelands_auth`");
    st->execute(
        "CREATE TABLE IF NOT EXISTS `firelands_auth`.`schema_migrations` ("
        "`migration` VARCHAR(255) NOT NULL,"
        "`applied_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY (`migration`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
  }

  static bool IsMigrationApplied(std::shared_ptr<sql::Connection> conn,
                                 const std::string &migrationFilename) {
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

  static void RecordMigrationApplied(std::shared_ptr<sql::Connection> conn,
                                     const std::string &migrationFilename) {
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

  /**
   * @brief Splits a string of SQL commands into individual statements.
   * Handles basic string literal escaping to avoid splitting inside strings.
   */
  static std::vector<std::string> SplitStatements(const std::string &sql) {
    std::vector<std::string> statements;
    std::string current;
    bool inString = false;
    char stringChar = 0;
    /// `--` to end of line: semicolons here must not split statements (e.g.
    /// comments mentioning "`auth` URL; `USE`" would otherwise produce garbage).
    bool inLineComment = false;

    for (size_t i = 0; i < sql.length(); ++i) {
      char c = sql[i];

      if (inLineComment) {
        current += c;
        if (c == '\n')
          inLineComment = false;
        continue;
      }

      if (!inString && c == '-' && i + 1 < sql.length() && sql[i + 1] == '-') {
        current += c;
        current += sql[++i];
        inLineComment = true;
        continue;
      }

      // Basic string handling (single quote, double quote, backtick)
      if ((c == '\'' || c == '"' || c == '`') &&
          (i == 0 || sql[i - 1] != '\\')) {
        if (!inString) {
          inString = true;
          stringChar = c;
        } else if (stringChar == c) {
          // SQL string escapes: '' and "" inside quoted literals must not end the
          // string (otherwise migrations like DEFAULT ''0'' split incorrectly).
          if (c == '\'' && i + 1 < sql.length() && sql[i + 1] == '\'') {
            ++i;
            continue;
          }
          if (c == '"' && i + 1 < sql.length() && sql[i + 1] == '"') {
            ++i;
            continue;
          }
          // Backtick-delimited identifiers: `` → one `
          if (c == '`' && i + 1 < sql.length() && sql[i + 1] == '`') {
            ++i;
            continue;
          }
          inString = false;
        }
      }

      if (c == ';' && !inString) {
        statements.push_back(current);
        current.clear();
      } else {
        current += c;
      }
    }
    if (!current.empty()) {
      statements.push_back(current);
    }
    return statements;
  }

  /**
   * @brief Executes a single SQL statement.
   */
  /// @return false if execution failed (caller may count migration failures).
  static bool ExecuteStatement(std::shared_ptr<sql::Connection> conn,
                               const std::string &statement) {
    std::string trimmed = statement;

    // Trim whitespace
    trimmed.erase(0, trimmed.find_first_not_of(" \n\r\t"));
    trimmed.erase(trimmed.find_last_not_of(" \n\r\t") + 1);

    // Skip empty statements or pure comments
    if (trimmed.empty() || trimmed.find("--") == 0 || trimmed.find("#") == 0 ||
        trimmed.find("/*") == 0) {
      return true;
    }

    try {
      std::unique_ptr<sql::Statement> stmnt(conn->createStatement());
      stmnt->execute(trimmed);
      return true;
    } catch (sql::SQLException &e) {
      int const code = e.getErrorCode();
      // Duplicate column / duplicate key name / table already exists (1050)
      if (code == 1060 || code == 1061 || code == 1050) {
        LOG_DEBUG("Migration statement skipped (already present): {}", e.what());
        return true;
      }
      LOG_WARN("Migration SQL failed ({}): {}", trimmed.substr(0, 80), e.what());
      return false;
    }
  }
};

} // namespace Firelands

#endif // FIRELANDS_INFRASTRUCTURE_PERSISTENCE_DATABASE_MIGRATOR_H
