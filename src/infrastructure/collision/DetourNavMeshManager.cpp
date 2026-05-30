#include <infrastructure/collision/DetourNavMeshManager.h>

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourCommon.h>
#include <DetourAlloc.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

namespace Firelands {

namespace {
constexpr float kTileSize = 533.33333f;
constexpr float kMapOrigin = -17066.66656f;
constexpr uint32_t kTileCountPerAxis = 64;
constexpr uint32_t kMmapMagic = 'M' | ('M' << 8) | ('A' << 16) | ('P' << 24);

struct MmapTileHeader {
  uint32_t mmapMagic;
  uint32_t dtVersion;
  uint32_t mmapSize;
  unsigned char usesLiquids;
  unsigned char padding[3];
};

void WowToDetour(float x, float y, float z, float out[3]) {
  out[0] = x;
  out[1] = z;
  out[2] = y;
}

Vec3 DetourToWow(float const* pos) {
  return Vec3{pos[0], pos[2], pos[1]};
}

} // namespace

DetourNavMeshManager::DetourNavMeshManager(std::string dataRoot,
                                           DetourNavMeshConfig config)
    : _dataRoot(std::move(dataRoot)), _config(std::move(config)) {}

DetourNavMeshManager::~DetourNavMeshManager() {
  for (auto& [mapId, entry] : _loadedMaps) {
    if (entry.navQuery) {
      dtFreeNavMeshQuery(entry.navQuery);
    }
    if (entry.navMesh) {
      dtFreeNavMesh(entry.navMesh);
    }
  }
  _loadedMaps.clear();
}

bool DetourNavMeshManager::ReadMmapTile(uint32_t mapId, uint32_t tileX,
                                         uint32_t tileY,
                                         dtNavMesh* navMesh) const {
  std::filesystem::path mmapDir =
      std::filesystem::path(_dataRoot) / "mmaps";
  std::string fileName =
      (mmapDir / (std::to_string(mapId) + "_" + std::to_string(tileX) + "_" +
                  std::to_string(tileY) + ".mmtile"))
          .string();

  FILE* file = fopen(fileName.c_str(), "rb");
  if (!file)
    return false;

  fseek(file, 0, SEEK_END);
  long fileSize = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (fileSize < static_cast<long>(sizeof(MmapTileHeader) + sizeof(dtMeshHeader))) {
    fclose(file);
    return false;
  }

  std::vector<unsigned char> data(fileSize);
  size_t readSize = fread(data.data(), 1, fileSize, file);
  fclose(file);

  if (readSize != static_cast<size_t>(fileSize))
    return false;

  MmapTileHeader const* mmapHeader =
      reinterpret_cast<MmapTileHeader const*>(data.data());
  if (mmapHeader->mmapMagic != kMmapMagic ||
      mmapHeader->dtVersion != DT_NAVMESH_VERSION ||
      mmapHeader->mmapSize == 0 ||
      sizeof(MmapTileHeader) + mmapHeader->mmapSize > data.size()) {
    return false;
  }

  unsigned char const* tilePayload = data.data() + sizeof(MmapTileHeader);
  dtMeshHeader const* header =
      reinterpret_cast<dtMeshHeader const*>(tilePayload);
  if (header->magic != DT_NAVMESH_MAGIC || header->version != DT_NAVMESH_VERSION)
    return false;

  auto* tileData =
      static_cast<unsigned char*>(dtAlloc(mmapHeader->mmapSize, DT_ALLOC_PERM));
  if (!tileData)
    return false;
  std::memcpy(tileData, tilePayload, mmapHeader->mmapSize);

  dtStatus status = navMesh->addTile(tileData, static_cast<int>(mmapHeader->mmapSize),
                                     DT_TILE_FREE_DATA, 0, nullptr);
  if (dtStatusFailed(status))
    dtFree(tileData);
  return dtStatusSucceed(status);
}

bool DetourNavMeshManager::LoadMapNavMesh(uint32_t mapId) {
  if (_loadedMaps.count(mapId) > 0)
    return true;

  dtNavMeshParams params{};
  float const navOrigin[3] = {kMapOrigin, 0.0f, kMapOrigin};
  dtVcopy(params.orig, navOrigin);
  params.tileWidth = kTileSize;
  params.tileHeight = kTileSize;
  params.maxTiles = static_cast<int>(kTileCountPerAxis * kTileCountPerAxis);
  params.maxPolys = 1 << 16;

  dtNavMesh* navMesh = dtAllocNavMesh();
  if (!navMesh)
    return false;

  dtStatus status = navMesh->init(&params);
  if (dtStatusFailed(status)) {
    dtFreeNavMesh(navMesh);
    return false;
  }

  bool anyTileLoaded = false;
  for (uint32_t ty = 0; ty < kTileCountPerAxis; ++ty) {
    for (uint32_t tx = 0; tx < kTileCountPerAxis; ++tx) {
      if (ReadMmapTile(mapId, tx, ty, navMesh))
        anyTileLoaded = true;
    }
  }

  if (!anyTileLoaded) {
    dtFreeNavMesh(navMesh);
    return false;
  }

  dtNavMeshQuery* navQuery = dtAllocNavMeshQuery();
  if (!navQuery) {
    dtFreeNavMesh(navMesh);
    return false;
  }

  status = navQuery->init(navMesh, _config.maxNavMeshNodes);
  if (dtStatusFailed(status)) {
    dtFreeNavMeshQuery(navQuery);
    dtFreeNavMesh(navMesh);
    return false;
  }

  _loadedMaps[mapId] = {navMesh, navQuery};
  return true;
}

void DetourNavMeshManager::UnloadMapNavMesh(uint32_t mapId) {
  auto it = _loadedMaps.find(mapId);
  if (it == _loadedMaps.end())
    return;

  if (it->second.navQuery)
    dtFreeNavMeshQuery(it->second.navQuery);
  if (it->second.navMesh)
    dtFreeNavMesh(it->second.navMesh);

  _loadedMaps.erase(it);
}

bool DetourNavMeshManager::IsNavMeshLoaded(uint32_t mapId) const {
  return _loadedMaps.count(mapId) > 0;
}

void DetourNavMeshManager::RemoveDuplicateWaypoints(
    std::vector<Vec3>& waypoints) {
  if (waypoints.size() <= 1)
    return;

  std::vector<Vec3> filtered;
  filtered.push_back(waypoints.front());

  for (size_t i = 1; i < waypoints.size(); ++i) {
    float const dx = waypoints[i].x - filtered.back().x;
    float const dy = waypoints[i].y - filtered.back().y;
    float const dz = waypoints[i].z - filtered.back().z;
    float const distSq = dx * dx + dy * dy + dz * dz;

    if (distSq > 0.5f * 0.5f)
      filtered.push_back(waypoints[i]);
  }

  if (filtered.back().x != waypoints.back().x ||
      filtered.back().y != waypoints.back().y ||
      filtered.back().z != waypoints.back().z) {
    filtered.push_back(waypoints.back());
  }

  waypoints = std::move(filtered);
}

void DetourNavMeshManager::SmoothPath(std::vector<Vec3>& waypoints) {
  if (waypoints.size() <= 2)
    return;

  constexpr size_t kMaxIterations = 4;
  constexpr float kSmoothFactor = 0.5f;

  for (size_t iter = 0; iter < kMaxIterations; ++iter) {
    for (size_t i = 1; i < waypoints.size() - 1; ++i) {
      waypoints[i].x =
          waypoints[i].x * kSmoothFactor +
          (waypoints[i - 1].x + waypoints[i + 1].x) * (0.5f - kSmoothFactor / 2.0f);
      waypoints[i].y =
          waypoints[i].y * kSmoothFactor +
          (waypoints[i - 1].y + waypoints[i + 1].y) * (0.5f - kSmoothFactor / 2.0f);
      waypoints[i].z =
          waypoints[i].z * kSmoothFactor +
          (waypoints[i - 1].z + waypoints[i + 1].z) * (0.5f - kSmoothFactor / 2.0f);
    }
  }
}

FindPathResult DetourNavMeshManager::FindPath(
    FindPathRequest const& req) const {
  FindPathResult result;
  result.status = FindPathStatus::NavMeshMissing;

  auto it = _loadedMaps.find(req.mapId);
  if (it == _loadedMaps.end())
    return result;

  dtNavMeshQuery* navQuery = it->second.navQuery;
  dtNavMesh const* navMesh = it->second.navMesh;
  if (!navQuery || !navMesh)
    return result;

  float const searchExtents[3] = {_config.maxSearchRadius,
                                   _config.maxSearchRadius,
                                   _config.maxSearchRadius};

  dtPolyRef startRef = 0;
  dtPolyRef endRef = 0;
  float startNearest[3]{};
  float endNearest[3]{};
  float startPos[3]{};
  float endPos[3]{};
  WowToDetour(req.startX, req.startY, req.startZ, startPos);
  WowToDetour(req.endX, req.endY, req.endZ, endPos);

  dtQueryFilter filter;
  filter.setIncludeFlags(0xFFFF);
  filter.setExcludeFlags(0);

  dtStatus status = navQuery->findNearestPoly(
      startPos, searchExtents, &filter, &startRef, startNearest);
  if (dtStatusFailed(status) || startRef == 0) {
    result.status = FindPathStatus::NoPath;
    return result;
  }

  status = navQuery->findNearestPoly(endPos, searchExtents, &filter,
                                     &endRef, endNearest);
  if (dtStatusFailed(status) || endRef == 0) {
    result.status = FindPathStatus::NoPath;
    return result;
  }

  dtPolyRef pathPolys[256];
  int pathCount = 0;
  float straightPath[256 * 3];
  unsigned char straightPathFlags[256];
  dtPolyRef straightPathPolys[256];
  int straightPathCount = 0;

  int const maxPathPolys = std::clamp(_config.maxPathPolys, 1, 256);
  status = navQuery->findPath(startRef, endRef, startNearest, endNearest,
                              &filter, pathPolys, &pathCount,
                              maxPathPolys);
  if (dtStatusFailed(status) || pathCount == 0) {
    result.status = FindPathStatus::NoPath;
    return result;
  }

  status = navQuery->findStraightPath(
      startNearest, endNearest, pathPolys, pathCount, straightPath,
      straightPathFlags, straightPathPolys, &straightPathCount,
      maxPathPolys, 0);
  if (dtStatusFailed(status) || straightPathCount == 0) {
    result.status = FindPathStatus::NoPath;
    return result;
  }

  result.waypoints.reserve(straightPathCount);
  for (int i = 0; i < straightPathCount; ++i) {
    result.waypoints.push_back(DetourToWow(&straightPath[i * 3]));
  }

  RemoveDuplicateWaypoints(result.waypoints);

  if (req.smoothPath)
    SmoothPath(result.waypoints);

  bool const reachedEnd =
      (pathCount > 0 && pathPolys[pathCount - 1] == endRef);
  result.status =
      reachedEnd ? FindPathStatus::Complete : FindPathStatus::Partial;

  if (!reachedEnd && !req.allowPartialPath) {
    result.waypoints.clear();
    result.status = FindPathStatus::NoPath;
  }

  return result;
}

} // namespace Firelands
