#include <infrastructure/collision/VMapManager2.h>
#include <infrastructure/collision/WorldModelRuntime.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace Firelands {

namespace {

constexpr float kTileSize = 533.33333f;
constexpr float kMapOrigin = -17066.66656f;

Vec3 Sub(Vec3 const& a, Vec3 const& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

float Dot(Vec3 const& a, Vec3 const& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 Normalize(Vec3 const& v) {
  float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
  if (len < 1e-8f) return {0, 0, 1};
  return {v.x / len, v.y / len, v.z / len};
}

std::vector<uint8_t> ReadEntireFile(std::string const& path) {
  std::vector<uint8_t> data;
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return data;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz > 0) {
    data.resize(sz);
    fread(data.data(), 1, sz, f);
  }
  fclose(f);
  return data;
}

} // namespace

struct VMapManager2::LoadedTile {
  uint32_t tileX = 0;
  uint32_t tileY = 0;
  std::vector<ModelSpawn> spawns;
  std::vector<std::unique_ptr<WorldModelRuntime>> models;
  bool loaded = false;

  bool Load(uint32_t mapId, std::string const& dataRoot, uint32_t tx,
            uint32_t ty) {
    tileX = tx;
    tileY = ty;

    std::string tilePath = std::filesystem::path(dataRoot) / "vmaps" /
                           (std::to_string(mapId) + "_" +
                            std::to_string(tileY) + "_" +
                            std::to_string(tileX) + ".vmtile");

    auto tileData = ReadEntireFile(tilePath);
    if (tileData.size() < 12)
      return false;

    char magic[9] = {};
    std::memcpy(magic, tileData.data(), 8);
    if (std::memcmp(magic, "VMAP_4.8", 8) != 0)
      return false;

    size_t offset = 8;
    uint32_t nSpawns = 0;
    std::memcpy(&nSpawns, tileData.data() + offset, 4);
    offset += 4;

    spawns.resize(nSpawns);
    for (uint32_t i = 0; i < nSpawns; ++i) {
      if (offset + 34 > tileData.size())
        break;
      ModelSpawn& sp = spawns[i];

      sp.flags = tileData[offset++];
      sp.adtId = tileData[offset++];
      std::memcpy(&sp.ID, tileData.data() + offset, 4);
      offset += 4;
      sp.iPos.x = 0; std::memcpy(&sp.iPos.x, tileData.data() + offset, 4); offset += 4;
      sp.iPos.y = 0; std::memcpy(&sp.iPos.y, tileData.data() + offset, 4); offset += 4;
      sp.iPos.z = 0; std::memcpy(&sp.iPos.z, tileData.data() + offset, 4); offset += 4;
      sp.iRot.x = 0; std::memcpy(&sp.iRot.x, tileData.data() + offset, 4); offset += 4;
      sp.iRot.y = 0; std::memcpy(&sp.iRot.y, tileData.data() + offset, 4); offset += 4;
      sp.iRot.z = 0; std::memcpy(&sp.iRot.z, tileData.data() + offset, 4); offset += 4;
      std::memcpy(&sp.iScale, tileData.data() + offset, 4);
      offset += 4;

      if (sp.flags & 0x02) {
        sp.iBound.lo.x = 0; std::memcpy(&sp.iBound.lo.x, tileData.data() + offset, 4); offset += 4;
        sp.iBound.lo.y = 0; std::memcpy(&sp.iBound.lo.y, tileData.data() + offset, 4); offset += 4;
        sp.iBound.lo.z = 0; std::memcpy(&sp.iBound.lo.z, tileData.data() + offset, 4); offset += 4;
        sp.iBound.hi.x = 0; std::memcpy(&sp.iBound.hi.x, tileData.data() + offset, 4); offset += 4;
        sp.iBound.hi.y = 0; std::memcpy(&sp.iBound.hi.y, tileData.data() + offset, 4); offset += 4;
        sp.iBound.hi.z = 0; std::memcpy(&sp.iBound.hi.z, tileData.data() + offset, 4); offset += 4;
      }

      uint32_t nameLen = 0;
      std::memcpy(&nameLen, tileData.data() + offset, 4);
      offset += 4;
      if (nameLen > 0 && offset + nameLen <= tileData.size()) {
        sp.name.assign(reinterpret_cast<char const*>(tileData.data() + offset), nameLen);
        offset += nameLen;
      }
    }

    for (auto const& sp : spawns) {
      if (sp.name.empty())
        continue;
      std::string modelPath = std::filesystem::path(dataRoot) / "vmaps" /
                              (sp.name + ".vmo");
      auto modelData = ReadEntireFile(modelPath);
      if (modelData.empty())
        continue;
      auto model = std::make_unique<WorldModelRuntime>();
      if (model->Read(modelData))
        models.push_back(std::move(model));
    }

    loaded = true;
    return true;
  }
};

struct VMapManager2::LoadedMap {
  uint32_t mapId = 0;
  std::string dataRoot;
  BoundingIntervalHierarchy treeBih;
  std::vector<std::pair<uint32_t, uint32_t>> spawnIndex;
  std::unordered_map<uint32_t, std::unique_ptr<LoadedTile>> tiles;
  bool loaded = false;

  bool Load(uint32_t id, std::string const& root) {
    mapId = id;
    dataRoot = root;

    std::string treePath = std::filesystem::path(dataRoot) / "vmaps" /
                           (std::to_string(mapId) + ".vmtree");
    auto treeData = ReadEntireFile(treePath);
    if (treeData.size() < 16)
      return false;

    char magic[9] = {};
    std::memcpy(magic, treeData.data(), 8);
    if (std::memcmp(magic, "VMAP_4.8", 8) != 0)
      return false;

    size_t offset = 8;
    while (offset + 4 <= treeData.size()) {
      char tag[5] = {};
      std::memcpy(tag, treeData.data() + offset, 4);
      offset += 4;

      if (std::memcmp(tag, "NODE", 4) == 0) {
        treeBih.Read(treeData, offset);
      } else if (std::memcmp(tag, "SIDX", 4) == 0) {
        if (offset + 4 > treeData.size())
          break;
        uint32_t mapSpawnsSize = 0;
        std::memcpy(&mapSpawnsSize, treeData.data() + offset, 4);
        offset += 4;
        spawnIndex.resize(mapSpawnsSize);
        for (uint32_t i = 0; i < mapSpawnsSize; ++i) {
          if (offset + 8 > treeData.size())
            break;
          uint32_t spawnId = 0;
          std::memcpy(&spawnId, treeData.data() + offset, 4);
          offset += 4;
          uint32_t treeIdx = 0;
          std::memcpy(&treeIdx, treeData.data() + offset, 4);
          offset += 4;
          spawnIndex[i] = {spawnId, treeIdx};
        }
      } else {
        if (offset + 4 > treeData.size())
          break;
        uint32_t chunkSize = 0;
        std::memcpy(&chunkSize, treeData.data() + offset, 4);
        offset += 4;
        offset += chunkSize;
      }
    }

    loaded = true;
    return true;
  }

  LoadedTile* GetTile(uint32_t tileX, uint32_t tileY) {
    uint32_t key = (tileX << 16) | tileY;
    auto it = tiles.find(key);
    if (it != tiles.end())
      return it->second.get();

    auto tile = std::make_unique<LoadedTile>();
    if (!tile->Load(mapId, dataRoot, tileX, tileY))
      return nullptr;
    auto* ptr = tile.get();
    tiles[key] = std::move(tile);
    return ptr;
  }
};

VMapManager2::VMapManager2() = default;
VMapManager2::~VMapManager2() = default;

bool VMapManager2::LoadMap(uint32_t mapId, std::string const& dataRoot) {
  if (_loadedMaps.count(mapId) > 0)
    return _loadedMaps[mapId]->loaded;

  auto map = std::make_unique<LoadedMap>();
  if (!map->Load(mapId, dataRoot))
    return false;
  _loadedMaps[mapId] = std::move(map);
  _currentMapId = mapId;
  return true;
}

void VMapManager2::UnloadMap(uint32_t mapId) {
  _loadedMaps.erase(mapId);
  if (_currentMapId == mapId)
    _currentMapId = 0;
}

bool VMapManager2::IsMapLoaded(uint32_t mapId) const {
  auto it = _loadedMaps.find(mapId);
  return it != _loadedMaps.end() && it->second->loaded;
}

uint32_t VMapManager2::WorldToTileX(float x) const {
  return static_cast<uint32_t>(std::floor((x - kMapOrigin) / kTileSize));
}

uint32_t VMapManager2::WorldToTileY(float y) const {
  return static_cast<uint32_t>(std::floor((y - kMapOrigin) / kTileSize));
}

float VMapManager2::TileOriginX(uint32_t tx) const {
  return kMapOrigin + static_cast<float>(tx) * kTileSize;
}

float VMapManager2::TileOriginY(uint32_t ty) const {
  return kMapOrigin + static_cast<float>(ty) * kTileSize;
}

VMapManager2::LoadedTile const* VMapManager2::GetTileForPosition(
    float x, float y) const {
  auto it = _loadedMaps.find(_currentMapId);
  if (it == _loadedMaps.end())
    return nullptr;

  uint32_t tx = WorldToTileX(x);
  uint32_t ty = WorldToTileY(y);
  return it->second->GetTile(tx, ty);
}

bool VMapManager2::LineOfSight(float x0, float y0, float z0, float x1,
                                float y1, float z1) const {
  auto it = _loadedMaps.find(_currentMapId);
  if (it == _loadedMaps.end())
    return true;

  Vec3 start = {x0, y0, z0};
  Vec3 end = {x1, y1, z1};
  Vec3 dir = Sub(end, start);
  float maxDist = std::sqrt(Dot(dir, dir));
  if (maxDist < 0.01f)
    return true;
  Vec3 rayDir = Normalize(dir);

  uint32_t startTX = WorldToTileX(x0);
  uint32_t startTY = WorldToTileY(y0);
  uint32_t endTX = WorldToTileX(x1);
  uint32_t endTY = WorldToTileY(y1);

  uint32_t minTX = std::min(startTX, endTX);
  uint32_t maxTX = std::max(startTX, endTX);
  uint32_t minTY = std::min(startTY, endTY);
  uint32_t maxTY = std::max(startTY, endTY);

  for (uint32_t ty = minTY; ty <= maxTY; ++ty) {
    for (uint32_t tx = minTX; tx <= maxTX; ++tx) {
      auto const* tile = it->second->GetTile(tx, ty);
      if (!tile)
        continue;

      for (auto const& model : tile->models) {
        for (auto const& group : model->GetGroups()) {
          float hitDist = maxDist;
          if (group.RayIntersects(start, rayDir, maxDist, hitDist))
            return false;
        }
      }
    }
  }

  return true;
}

float VMapManager2::GetHeight(float x, float y, float zHint) const {
  auto const* tile = GetTileForPosition(x, y);
  if (!tile)
    return zHint;

  float bestZ = -1e30f;
  for (auto const& model : tile->models) {
    for (auto const& group : model->GetGroups()) {
      float h = group.GetHeightAt(x, y);
      if (h > bestZ)
        bestZ = h;
    }
  }

  if (bestZ > -1e29f)
    return bestZ;
  return zHint;
}

} // namespace Firelands
