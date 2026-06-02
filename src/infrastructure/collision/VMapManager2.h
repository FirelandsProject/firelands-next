#ifndef FIRELANDS_INFRASTRUCTURE_COLLISION_VMAP_MANAGER2_H
#define FIRELANDS_INFRASTRUCTURE_COLLISION_VMAP_MANAGER2_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Firelands {

class WorldModelRuntime;

class VMapManager2 {
public:
  VMapManager2();
  ~VMapManager2();

  VMapManager2(VMapManager2 const&) = delete;
  VMapManager2& operator=(VMapManager2 const&) = delete;

  bool LoadMap(uint32_t mapId, std::string const& dataRoot);
  void UnloadMap(uint32_t mapId);
  bool IsMapLoaded(uint32_t mapId) const;

  bool LineOfSight(float x0, float y0, float z0, float x1, float y1,
                   float z1) const;
  float GetHeight(float x, float y, float zHint) const;

private:
  struct LoadedMap;
  struct LoadedTile;

  LoadedTile const* GetTileForPosition(float x, float y) const;
  uint32_t WorldToTileX(float x) const;
  uint32_t WorldToTileY(float y) const;
  float TileOriginX(uint32_t tx) const;
  float TileOriginY(uint32_t ty) const;

  std::unordered_map<uint32_t, std::unique_ptr<LoadedMap>> _loadedMaps;
  uint32_t _currentMapId = 0;
};

} // namespace Firelands

#endif
