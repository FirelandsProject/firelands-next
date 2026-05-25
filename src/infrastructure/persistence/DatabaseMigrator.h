#ifndef FIRELANDS_INFRASTRUCTURE_PERSISTENCE_DATABASE_MIGRATOR_H
#define FIRELANDS_INFRASTRUCTURE_PERSISTENCE_DATABASE_MIGRATOR_H

#include <infrastructure/persistence/DatabaseMigrationCatalog.h>
#include <string>

namespace Firelands {

/**
 * @brief Applies init schema and incremental migrations for auth or world startup.
 *
 * Bundled SQL under sql/bundled/ is for Docker first boot only — not executed here.
 * Fresh database: sql/init for the role's databases, then pending sql/migrations.
 * Existing database: pending sql/migrations only.
 * Applied files are tracked in firelands_auth.schema_migrations.
 */
class DatabaseMigrator {
public:
  /// Auth server: firelands_auth init + migrations only.
  static void MigrateAuthServerStartup(const std::string &authUri,
                                       const std::string &user,
                                       const std::string &password,
                                       const std::string &sqlDir);

  /// World server: firelands_characters and firelands_world init + migrations only.
  static void MigrateWorldServerStartup(const std::string &bookkeepingAuthUri,
                                        const std::string &charactersUri,
                                        const std::string &worldUri,
                                        const std::string &user,
                                        const std::string &password,
                                        const std::string &sqlDir);

  /// @return Number of statements that failed to execute (0 = full success).
  static size_t Migrate(const std::string &uri, const std::string &user,
                        const std::string &password,
                        const std::string &sqlFilePath);

private:
  static void MigrateForRole(MigrationServerRole role,
                             const std::string &bookkeepingAuthUri,
                             const std::string &charactersUri,
                             const std::string &worldUri,
                             const std::string &user,
                             const std::string &password,
                             const std::string &sqlDir);

  static std::string UriForDatabaseInRole(std::string const &database,
                                          std::string const &serverUri,
                                          std::string const &authUri,
                                          std::string const &charactersUri,
                                          std::string const &worldUri);
};

} // namespace Firelands

#endif // FIRELANDS_INFRASTRUCTURE_PERSISTENCE_DATABASE_MIGRATOR_H
