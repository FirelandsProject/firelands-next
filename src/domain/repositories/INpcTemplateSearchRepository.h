#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Firelands {

struct NpcTemplateSearchRow {
  uint32_t entry = 0;
  std::string name;
  std::string subname;
};

/// Read-only lookup for GM tooling (e.g. `.npc search`). Backed by
/// `firelands_world.creature_template` when populated from client data.
class INpcTemplateSearchRepository {
public:
  virtual ~INpcTemplateSearchRepository() = default;

  /// Case-insensitive substring match on `name` or `subname` (ASCII-ish).
  virtual std::vector<NpcTemplateSearchRow> SearchNameSubstring(
      std::string const &sanitizedQuery, uint32_t limit,
      uint32_t offset) const = 0;
};

} // namespace Firelands
