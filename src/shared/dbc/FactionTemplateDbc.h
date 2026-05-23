#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace Firelands {

/// One row of client `FactionTemplate.dbc` (build 15595 / `FactionTemplateEntryfmt`).
struct FactionTemplateEntry {
  uint32_t id = 0;
  /// Primary `Faction.dbc` id for reputation / forced reactions.
  uint32_t faction = 0;
  uint32_t flags = 0;
  uint32_t factionGroup = 0;
  uint32_t friendGroup = 0;
  uint32_t enemyGroup = 0;
  uint32_t enemies[4]{};
  uint32_t friendFactions[4]{};
};

/// Read-only store for `FactionTemplate.dbc` under `Data.DbcPath`.
class FactionTemplateDbc {
public:
  bool Load(std::string const &path);

  bool IsLoaded() const { return m_loaded; }
  std::size_t RowCount() const { return m_byId.size(); }

  bool HasEntry(uint32_t templateId) const;

  /// Empty when not loaded or unknown id.
  std::optional<FactionTemplateEntry> TryGet(uint32_t templateId) const;

  /// Unit tests only: seed a row without loading a `.dbc` file.
  void InjectEntryForTest(uint32_t templateId, FactionTemplateEntry entry);

private:
  bool m_loaded = false;
  std::unordered_map<uint32_t, FactionTemplateEntry> m_byId;
};

} // namespace Firelands
