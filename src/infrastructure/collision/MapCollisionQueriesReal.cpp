#include <infrastructure/collision/MapCollisionQueriesReal.h>

#include <shared/Logger.h>

namespace Firelands {

MapCollisionQueriesReal::MapCollisionQueriesReal(std::string dataRoot)
    : _navMeshManager(dataRoot), _vmapManager() {}

bool MapCollisionQueriesReal::IsNavMeshDataAvailable(uint32_t mapId) const {
  if (_navMeshManager.IsNavMeshLoaded(mapId))
    return true;
  return _navMeshManager.LoadMapNavMesh(mapId);
}

bool MapCollisionQueriesReal::LineOfSight(uint32_t mapId, float x0, float y0,
                                           float z0, float x1, float y1,
                                           float z1) const {
  if (_navMeshManager.HasDataRoot() && !_vmapManager.IsMapLoaded(mapId))
    _vmapManager.LoadMap(mapId, _navMeshManager.GetDataRoot());
  if (_vmapManager.IsMapLoaded(mapId))
    return _vmapManager.LineOfSight(x0, y0, z0, x1, y1, z1);
  return true;
}

FindPathResult MapCollisionQueriesReal::FindPath(
    FindPathRequest const& req) const {
  if (!_navMeshManager.IsNavMeshLoaded(req.mapId)) {
    if (!_navMeshManager.LoadMapNavMesh(req.mapId)) {
      LOG_WARN("MMAP path request could not load navmesh: mapId={} start=({}, {}, {}) end=({}, {}, {})",
               req.mapId, req.startX, req.startY, req.startZ, req.endX, req.endY,
               req.endZ);
      FindPathResult result;
      result.status = FindPathStatus::NavMeshMissing;
      return result;
    }
  }
  return _navMeshManager.FindPath(req);
}

float MapCollisionQueriesReal::GetHeight(uint32_t mapId, float x, float y,
                                          float zHint) const {
  if (_navMeshManager.HasDataRoot() && !_vmapManager.IsMapLoaded(mapId))
    _vmapManager.LoadMap(mapId, _navMeshManager.GetDataRoot());
  if (_vmapManager.IsMapLoaded(mapId))
    return _vmapManager.GetHeight(x, y, zHint);
  return zHint;
}

} // namespace Firelands
