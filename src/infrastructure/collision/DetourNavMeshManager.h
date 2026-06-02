#ifndef FIRELANDS_INFRASTRUCTURE_COLLISION_DETOUR_NAV_MESH_MANAGER_H
#define FIRELANDS_INFRASTRUCTURE_COLLISION_DETOUR_NAV_MESH_MANAGER_H

#include <application/ports/IMapCollisionQueries.h>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class dtNavMesh;
class dtNavMeshQuery;

namespace Firelands {

struct DetourNavMeshConfig {
  float maxSearchRadius = 100.0f;
  int maxPathPolys = 256;
  int maxNavMeshNodes = 4096;
};

class DetourNavMeshManager {
public:
  explicit DetourNavMeshManager(std::string dataRoot,
                                DetourNavMeshConfig config = {});
  ~DetourNavMeshManager();

  DetourNavMeshManager(DetourNavMeshManager const&) = delete;
  DetourNavMeshManager& operator=(DetourNavMeshManager const&) = delete;

  bool LoadMapNavMesh(uint32_t mapId);
  void UnloadMapNavMesh(uint32_t mapId);
  bool IsNavMeshLoaded(uint32_t mapId) const;
  uint32_t GetLoadedMapCount() const;
  uint32_t GetLoadedTileCount() const;
  std::vector<std::pair<uint32_t, uint32_t>> GetLoadedTiles(uint32_t mapId) const;
  bool HasDataRoot() const { return !_dataRoot.empty(); }
  std::string const& GetDataRoot() const { return _dataRoot; }

  FindPathResult FindPath(FindPathRequest const& req) const;

  /// Sample the navmesh ground height at WoW (x, y). Searches with a wide
  /// vertical extent so callers above the terrain (flying players) still
  /// resolve the floor underneath. Returns true and writes the height into
  /// `outZ` when a poly is found; false otherwise.
  bool GetNavMeshHeight(uint32_t mapId, float x, float y, float zHint,
                        float& outZ) const;

private:
  struct MapNavMesh {
    dtNavMesh* navMesh = nullptr;
    dtNavMeshQuery* navQuery = nullptr;
    std::vector<std::pair<uint32_t, uint32_t>> loadedTiles;
  };

  bool ReadMmapTile(uint32_t mapId, uint32_t tileX, uint32_t tileY,
                    dtNavMesh* navMesh) const;
  static void RemoveDuplicateWaypoints(std::vector<Vec3>& waypoints);
  static void SmoothPath(std::vector<Vec3>& waypoints);

  std::string _dataRoot;
  DetourNavMeshConfig _config;
  std::unordered_map<uint32_t, MapNavMesh> _loadedMaps;
};

} // namespace Firelands

#endif
