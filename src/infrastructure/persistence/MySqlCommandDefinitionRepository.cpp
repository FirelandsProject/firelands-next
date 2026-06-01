#include <infrastructure/persistence/MySqlCommandDefinitionRepository.h>

#include <conncpp.hpp>
#include <memory>
#include <vector>

namespace Firelands {

MySqlCommandDefinitionRepository::MySqlCommandDefinitionRepository(
    std::shared_ptr<sql::Connection> conn)
    : _conn(std::move(conn)) {}

std::vector<CommandDefinition> MySqlCommandDefinitionRepository::LoadAll() {
  std::vector<CommandDefinition> result;
  if (!_conn)
    return result;

  try {
    std::unique_ptr<sql::Statement> stmt(_conn->createStatement());
    std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery(
        "SELECT name, description, syntax, min_access_level "
        "FROM firelands_commands ORDER BY min_access_level, name"));

    while (rs->next()) {
      CommandDefinition def;
      def.name = rs->getString(1);
      def.description = rs->getString(2);
      def.syntax = rs->getString(3);
      def.minAccessLevel = static_cast<uint8_t>(rs->getUInt(4));
      result.push_back(std::move(def));
    }
  } catch (sql::SQLException &e) {
    (void)e;
  }

  return result;
}

} // namespace Firelands
