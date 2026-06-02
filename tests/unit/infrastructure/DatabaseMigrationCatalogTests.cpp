#include <infrastructure/persistence/DatabaseMigrationCatalog.h>

#include <gtest/gtest.h>

namespace Firelands {
namespace {

TEST(DatabaseMigrationCatalogTests, DetectFromUseStatement) {
  std::string const sql = "USE `firelands_world`;\nCREATE TABLE t (id INT);";
  auto const db = DetectMigrationTargetDatabase("99_misc.sql", sql);
  ASSERT_TRUE(db.has_value());
  EXPECT_EQ(*db, "firelands_world");
}

TEST(DatabaseMigrationCatalogTests, DetectFromFilenameWorldPrefix) {
  auto const db = DetectMigrationTargetDatabase("36_world_quest_gossip_tables.sql", "");
  ASSERT_TRUE(db.has_value());
  EXPECT_EQ(*db, "firelands_world");
}

TEST(DatabaseMigrationCatalogTests, DetectCharactersFromQualifiedTable) {
  std::string const sql =
      "ALTER TABLE `firelands_characters`.`character_spell` ADD COLUMN x INT;";
  auto const db = DetectMigrationTargetDatabase("7_spell.sql", sql);
  ASSERT_TRUE(db.has_value());
  EXPECT_EQ(*db, "firelands_characters");
}

TEST(DatabaseMigrationCatalogTests, AuthRoleTargetsOnlyAuth) {
  auto const dbs = TargetDatabasesForRole(MigrationServerRole::Auth);
  ASSERT_EQ(dbs.size(), 1U);
  EXPECT_EQ(dbs[0], kAuthDatabase);
}

TEST(DatabaseMigrationCatalogTests, WorldRoleTargetsAuthCharactersAndWorld) {
  auto const dbs = TargetDatabasesForRole(MigrationServerRole::World);
  ASSERT_EQ(dbs.size(), 3U);
  EXPECT_EQ(dbs[0], kAuthDatabase);
  EXPECT_EQ(dbs[1], kCharactersDatabase);
  EXPECT_EQ(dbs[2], kWorldDatabase);
}

TEST(DatabaseMigrationCatalogTests, InitPathsForWorldInOrder) {
  std::filesystem::path const sqlDir = "sql";
  auto const paths = InitScriptPathsForDatabase(sqlDir, kWorldDatabase);
  ASSERT_GE(paths.size(), 1U);
  EXPECT_EQ(paths.front().filename().string(), "world_schema.sql");
  EXPECT_EQ(paths.back().filename().string(), "world_seed.sql");
}

TEST(DatabaseMigrationCatalogTests, ExtractJdbcServerUriStripsDatabase) {
  EXPECT_EQ(ExtractJdbcServerUri("jdbc:mariadb://localhost:3306/firelands_auth"),
            "jdbc:mariadb://localhost:3306");
}

} // namespace
} // namespace Firelands
