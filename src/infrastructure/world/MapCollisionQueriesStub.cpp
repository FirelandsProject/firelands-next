#include <infrastructure/world/MapCollisionQueriesStub.h>

namespace Firelands {

MapCollisionQueriesStub::MapCollisionQueriesStub(std::string dataRoot)
    : _dataRoot(std::move(dataRoot)) {}

bool MapCollisionQueriesStub::IsNavMeshDataAvailable(uint32_t /*mapId*/) const {
  return !_dataRoot.empty();
}

uint32_t MapCollisionQueriesStub::GetLoadedMapCount() const {
  return 0;
}

uint32_t MapCollisionQueriesStub::GetLoadedTileCount() const {
  return 0;
}

std::vector<std::pair<uint32_t, uint32_t>> MapCollisionQueriesStub::GetLoadedTiles(
    uint32_t /*mapId*/) const {
  return {};
}

bool MapCollisionQueriesStub::LineOfSight(uint32_t /*mapId*/, float /*x0*/,
                                           float /*y0*/, float /*z0*/,
                                           float /*x1*/, float /*y1*/,
                                           float /*z1*/) const {
  (void)_dataRoot;
  return true;
}

FindPathResult MapCollisionQueriesStub::FindPath(
    FindPathRequest const& req) const {
  (void)_dataRoot;
  FindPathResult result;
  result.status = FindPathStatus::NavMeshMissing;

  if (!_dataRoot.empty()) {
    result.waypoints.push_back(
        Vec3{req.startX, req.startY, req.startZ});
    result.waypoints.push_back(Vec3{req.endX, req.endY, req.endZ});
    result.status = FindPathStatus::Complete;
  }
  return result;
}

float MapCollisionQueriesStub::GetHeight(uint32_t /*mapId*/, float /*x*/,
                                          float /*y*/, float zHint) const {
  (void)_dataRoot;
  return zHint;
}

} // namespace Firelands
