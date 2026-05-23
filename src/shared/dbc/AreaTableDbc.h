#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Firelands {

/// Minimal `AreaTable.dbc` — hierarchy and map binding for phasing (Cata 4.3.4).
class AreaTableDbc {
public:
  bool Load(std::string const &path);
  bool IsLoaded() const { return m_loaded; }
  uint32_t GetParentAreaId(uint32_t areaId) const;
  uint32_t GetMapId(uint32_t areaId) const;
  /// Best area id for `phase_area` on `mapId` (validates `clientAreaHint` against map).
  uint32_t ResolveAreaForPhasing(uint32_t mapId, uint32_t clientAreaHint) const;

private:
  bool m_loaded = false;
  std::unordered_map<uint32_t, uint32_t> m_parentByAreaId;
  std::unordered_map<uint32_t, uint32_t> m_mapIdByAreaId;
  std::unordered_map<uint32_t, std::vector<uint32_t>> m_areaIdsByMapId;
};

} // namespace Firelands
