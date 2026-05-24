#include <application/services/MapService.h>

namespace Firelands {

MapService::MapService(uint32 mapId, std::shared_ptr<Map> map)
    : m_mapId(mapId), m_map(std::move(map)) {}

uint32 MapService::MapId() const { return m_mapId; }

Map const *MapService::GetMap() const { return m_map.get(); }

Map *MapService::GetMap() { return m_map.get(); }

std::shared_ptr<Map> MapService::SharedMap() const { return m_map; }

void MapService::RecordTick(double tickMs) {
  std::lock_guard<std::mutex> lock(m_snapshotMutex);
  m_lastTickTimeMs = tickMs;
  if (!m_hasTickSample) {
    m_avgTickTimeMs = tickMs;
    m_hasTickSample = true;
    return;
  }
  constexpr double kAlpha = 0.1;
  m_avgTickTimeMs = kAlpha * tickMs + (1.0 - kAlpha) * m_avgTickTimeMs;
}

MapSnapshot MapService::Snapshot() const {
  MapSnapshot snap = m_map ? m_map->CreateSnapshot() : MapSnapshot{};
  snap.mapId = m_mapId;
  {
    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    snap.avgTickTimeMs = m_avgTickTimeMs;
    snap.lastTickTimeMs = m_lastTickTimeMs;
  }
  return snap;
}

} // namespace Firelands
