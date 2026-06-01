#ifndef FIRELANDS_DOMAIN_MODELS_RBAC_ROLE_H
#define FIRELANDS_DOMAIN_MODELS_RBAC_ROLE_H

#include <cstdint>
#include <string>

namespace Firelands {

struct RbacRole {
  uint32_t id = 0;
  std::string name;
  uint64_t permissionMask = 0;
};

} // namespace Firelands

#endif // FIRELANDS_DOMAIN_MODELS_RBAC_ROLE_H
