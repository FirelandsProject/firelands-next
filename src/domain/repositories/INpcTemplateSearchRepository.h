#pragma once

#include <domain/models/NpcTemplate.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Firelands {

/// Read-only `creature_template` lookups (GM search, creature query, gossip).
class INpcTemplateSearchRepository {
public:
  virtual ~INpcTemplateSearchRepository() = default;

  virtual std::vector<NpcTemplate> SearchNameSubstring(std::string const &sanitizedQuery,
                                                       uint32_t limit,
                                                       uint32_t offset) const = 0;

  virtual std::optional<NpcTemplate> TryGetByEntry(uint32_t entry) const = 0;
};

} // namespace Firelands
