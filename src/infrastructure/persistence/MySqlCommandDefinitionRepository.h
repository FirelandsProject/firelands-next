#ifndef FIRELANDS_INFRASTRUCTURE_PERSISTENCE_MYSQL_COMMAND_DEFINITION_REPOSITORY_H
#define FIRELANDS_INFRASTRUCTURE_PERSISTENCE_MYSQL_COMMAND_DEFINITION_REPOSITORY_H

#include <domain/repositories/ICommandDefinitionRepository.h>
#include <memory>

namespace sql {
class Connection;
}

namespace Firelands {

class MySqlCommandDefinitionRepository : public ICommandDefinitionRepository {
public:
  explicit MySqlCommandDefinitionRepository(std::shared_ptr<sql::Connection> conn);
  std::vector<CommandDefinition> LoadAll() override;

private:
  std::shared_ptr<sql::Connection> _conn;
};

} // namespace Firelands

#endif
