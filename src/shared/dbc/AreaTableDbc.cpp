#include <shared/dbc/AreaTableDbc.h>

#include <shared/dbc/DbcReader.h>
#include <shared/Logger.h>

#include <string_view>
#include <vector>

namespace Firelands {

namespace {

constexpr std::string_view kAreaTableFmt = "niiiiiiiiiisiiiiiffiiiiiii";
constexpr size_t kFieldMapId = 1;
constexpr size_t kFieldParentAreaId = 2;

} // namespace

bool AreaTableDbc::Load(std::string const &path) {
  m_loaded = false;
  m_parentByAreaId.clear();
  m_mapIdByAreaId.clear();
  m_areaIdsByMapId.clear();

  DbcReader reader;
  if (!reader.Load(path)) {
    return false;
  }

  if (!reader.VerifyFormat(kAreaTableFmt)) {
    LOG_WARN("AreaTable.dbc layout mismatch in {}", path);
    return false;
  }

  std::vector<uint32_t> const offsets = DbcBuildFieldByteOffsets(kAreaTableFmt);
  for (uint32_t ri = 0; ri < reader.GetRecordCount(); ++ri) {
    uint32_t const id = reader.ReadUInt32(ri, 0, offsets);
    if (id == 0)
      continue;
    uint32_t const mapId = reader.ReadUInt32(ri, kFieldMapId, offsets);
    uint32_t const parent = reader.ReadUInt32(ri, kFieldParentAreaId, offsets);
    m_mapIdByAreaId[id] = mapId;
    m_parentByAreaId[id] = parent;
    m_areaIdsByMapId[mapId].push_back(id);
  }

  m_loaded = true;
  LOG_INFO("AreaTable.dbc: loaded {} area row(s) from {}", m_parentByAreaId.size(), path);
  return true;
}

uint32_t AreaTableDbc::GetParentAreaId(uint32_t areaId) const {
  if (!m_loaded || areaId == 0)
    return 0;
  auto const it = m_parentByAreaId.find(areaId);
  if (it == m_parentByAreaId.end())
    return 0;
  return it->second;
}

uint32_t AreaTableDbc::GetMapId(uint32_t areaId) const {
  if (!m_loaded || areaId == 0)
    return 0;
  auto const it = m_mapIdByAreaId.find(areaId);
  if (it == m_mapIdByAreaId.end())
    return 0;
  return it->second;
}

uint32_t AreaTableDbc::ResolveAreaForPhasing(uint32_t mapId, uint32_t clientAreaHint) const {
  if (!m_loaded || mapId == 0)
    return clientAreaHint;

  auto areaOnMap = [&](uint32_t areaId) {
    return areaId != 0 && GetMapId(areaId) == mapId;
  };

  if (areaOnMap(clientAreaHint))
    return clientAreaHint;

  uint32_t current = clientAreaHint;
  for (size_t depth = 0; depth < 32 && current != 0; ++depth) {
    current = GetParentAreaId(current);
    if (areaOnMap(current))
      return current;
  }

  auto const it = m_areaIdsByMapId.find(mapId);
  if (it != m_areaIdsByMapId.end() && !it->second.empty())
    return it->second.front();

  return clientAreaHint;
}

} // namespace Firelands
