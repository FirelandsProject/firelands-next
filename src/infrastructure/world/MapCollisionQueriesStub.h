#ifndef FIRELANDS_INFRASTRUCTURE_WORLD_MAP_COLLISION_QUERIES_STUB_H
#define FIRELANDS_INFRASTRUCTURE_WORLD_MAP_COLLISION_QUERIES_STUB_H

#include <application/ports/IMapCollisionQueries.h>
#include <string>

namespace Firelands {

/// Placeholder until vmap/mmap from reference implementation extractors are integrated.
class MapCollisionQueriesStub final : public IMapCollisionQueries {
public:
  explicit MapCollisionQueriesStub(std::string dataRoot = {});

  bool IsNavMeshDataAvailable(uint32_t mapId) const override;
  bool LineOfSight(uint32_t mapId, float x0, float y0, float z0, float x1,
                   float y1, float z1) const override;

private:
  std::string _dataRoot;
};

} // namespace Firelands

#endif
