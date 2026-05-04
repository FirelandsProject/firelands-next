#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Firelands {

struct NpcTemplateSearchRow {
  uint32_t entry = 0;
  std::string name;
  std::string subname;
  /// `creature_template.faction` (FactionTemplate.dbc); 0 if column null / missing.
  uint32_t factionTemplate = 0;
  /// For `SMSG_CREATURE_QUERY_RESPONSE` model slots; slot 0 derived from
  /// `creature.modelid` when present, otherwise a safe client fallback.
  std::array<uint32_t, 4> displayIds{};
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

  /// Exact template lookup (`creature_template.entry`). Used for
  /// `CMSG_CREATURE_QUERY` → nameplate/tooltips on the client.
  virtual std::optional<NpcTemplateSearchRow> TryGetByEntry(
      uint32_t entry) const = 0;
};

} // namespace Firelands
