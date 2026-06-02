#ifndef FIRELANDS_APPLICATION_PORTS_I_MAP_COLLISION_QUERIES_H
#define FIRELANDS_APPLICATION_PORTS_I_MAP_COLLISION_QUERIES_H

#include <cstdint>
#include <utility>
#include <vector>

namespace Firelands {

struct Vec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

enum class FindPathStatus : uint8_t {
  Complete,
  Partial,
  NoPath,
  NavMeshMissing
};

struct FindPathRequest {
  uint32_t mapId = 0;
  float startX = 0.0f;
  float startY = 0.0f;
  float startZ = 0.0f;
  float endX = 0.0f;
  float endY = 0.0f;
  float endZ = 0.0f;
  bool smoothPath = true;
  bool allowPartialPath = true;
};

struct FindPathResult {
  std::vector<Vec3> waypoints;
  FindPathStatus status = FindPathStatus::NoPath;
};

/// Port for mmap/vmap-backed queries (LoS, height, navmesh). Stub implementation
/// returns permissive values until extracted data is wired.
class IMapCollisionQueries {
public:
  virtual ~IMapCollisionQueries() = default;

  /// When false, callers should assume no navmesh files are loaded for `mapId`.
  virtual bool IsNavMeshDataAvailable(uint32_t mapId) const = 0;

  /// Total loaded navmesh maps.
  virtual uint32_t GetLoadedMapCount() const = 0;

  /// Total loaded navmesh tiles across all maps.
  virtual uint32_t GetLoadedTileCount() const = 0;

  /// Loaded tile coordinates for one map.
  virtual std::vector<std::pair<uint32_t, uint32_t>> GetLoadedTiles(
      uint32_t mapId) const = 0;

  /// Line of sight between two world points. Stub returns true (open line).
  virtual bool LineOfSight(uint32_t mapId, float x0, float y0, float z0,
                           float x1, float y1, float z1) const = 0;

  /// Navmesh pathfinding. Returns waypoints from start to end following the
  /// navmesh. When no navmesh data is available, returns NavMeshMissing.
  virtual FindPathResult FindPath(FindPathRequest const& req) const = 0;

  /// Height at (x,y) on the collision mesh. Returns z if known, otherwise
  /// returns the fallback `zHint`.
  virtual float GetHeight(uint32_t mapId, float x, float y,
                          float zHint) const = 0;
};

} // namespace Firelands

#endif
