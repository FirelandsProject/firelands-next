#ifndef FIRELANDS_DOMAIN_REPOSITORIES_I_COMMAND_DEFINITION_REPOSITORY_H
#define FIRELANDS_DOMAIN_REPOSITORIES_I_COMMAND_DEFINITION_REPOSITORY_H

#include <cstdint>
#include <string>
#include <vector>

namespace Firelands {

struct CommandDefinition {
  std::string name;
  std::string description;
  std::string syntax;
  uint64_t requiredPermissionMask = 0;
};

class ICommandDefinitionRepository {
public:
  virtual ~ICommandDefinitionRepository() = default;
  virtual std::vector<CommandDefinition> LoadAll() = 0;
};

} // namespace Firelands

#endif
