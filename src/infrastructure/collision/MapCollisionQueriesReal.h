#ifndef FIRELANDS_INFRASTRUCTURE_COLLISION_MAP_COLLISION_QUERIES_REAL_H
#define FIRELANDS_INFRASTRUCTURE_COLLISION_MAP_COLLISION_QUERIES_REAL_H

#include <application/ports/IMapCollisionQueries.h>
#include <infrastructure/collision/DetourNavMeshManager.h>
#include <infrastructure/collision/VMapManager2.h>
#include <memory>
#include <string>

namespace Firelands {

class MapCollisionQueriesReal final : public IMapCollisionQueries {
public:
  explicit MapCollisionQueriesReal(std::string dataRoot);
  ~MapCollisionQueriesReal() override = default;

  bool IsNavMeshDataAvailable(uint32_t mapId) const override;
  uint32_t GetLoadedMapCount() const override;
  uint32_t GetLoadedTileCount() const override;
  std::vector<std::pair<uint32_t, uint32_t>> GetLoadedTiles(uint32_t mapId) const override;
  bool LineOfSight(uint32_t mapId, float x0, float y0, float z0, float x1,
                   float y1, float z1) const override;
  FindPathResult FindPath(FindPathRequest const& req) const override;
  float GetHeight(uint32_t mapId, float x, float y,
                  float zHint) const override;

private:
  mutable DetourNavMeshManager _navMeshManager;
  mutable VMapManager2 _vmapManager;
};

} // namespace Firelands

#endif
