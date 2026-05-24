#pragma once

#include <domain/world/Map.h>
#include <domain/world/MapSnapshot.h>
#include <memory>
#include <mutex>

namespace Firelands {

/// Supervisory wrapper around a live map (tick timing + snapshots).
class MapService {
public:
  MapService(uint32 mapId, std::shared_ptr<Map> map);

  uint32 MapId() const;
  Map const *GetMap() const;
  Map *GetMap();
  std::shared_ptr<Map> SharedMap() const;
  MapSnapshot Snapshot() const;
  void RecordTick(double tickMs);

private:
  uint32 m_mapId;
  std::shared_ptr<Map> m_map;
  mutable std::mutex m_snapshotMutex;
  double m_avgTickTimeMs = 0.0;
  double m_lastTickTimeMs = 0.0;
  bool m_hasTickSample = false;
};

} // namespace Firelands
