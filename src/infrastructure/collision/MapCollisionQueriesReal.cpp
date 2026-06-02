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

uint32_t MapCollisionQueriesReal::GetLoadedMapCount() const {
  return _navMeshManager.GetLoadedMapCount();
}

uint32_t MapCollisionQueriesReal::GetLoadedTileCount() const {
  return _navMeshManager.GetLoadedTileCount();
}

std::vector<std::pair<uint32_t, uint32_t>> MapCollisionQueriesReal::GetLoadedTiles(
    uint32_t mapId) const {
  return _navMeshManager.GetLoadedTiles(mapId);
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
      LOG_MMAP_WARN("MMAP path request could not load navmesh: mapId={} start=({}, {}, {}) end=({}, {}, {})",
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
  // Prefer the navmesh ground height — it reflects walkable terrain (what AI
  // should track) and is available wherever creatures path. Fall back to the
  // raw vmap surface, then to the caller's hint.
  if (_navMeshManager.IsNavMeshLoaded(mapId) ||
      _navMeshManager.LoadMapNavMesh(mapId)) {
    float navHeight = 0.0f;
    if (_navMeshManager.GetNavMeshHeight(mapId, x, y, zHint, navHeight))
      return navHeight;
  }

  if (_navMeshManager.HasDataRoot() && !_vmapManager.IsMapLoaded(mapId))
    _vmapManager.LoadMap(mapId, _navMeshManager.GetDataRoot());
  if (_vmapManager.IsMapLoaded(mapId))
    return _vmapManager.GetHeight(x, y, zHint);
  return zHint;
}

} // namespace Firelands
