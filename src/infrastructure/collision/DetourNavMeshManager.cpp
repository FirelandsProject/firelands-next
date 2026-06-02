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
#include <limits>
#include <vector>

#include <shared/Logger.h>

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

uint32_t RepairMissingPolyFlags(unsigned char* tilePayload) {
  auto* header = reinterpret_cast<dtMeshHeader*>(tilePayload);
  unsigned char* cursor = tilePayload + dtAlign4(sizeof(dtMeshHeader));
  cursor += dtAlign4(sizeof(float) * 3 * header->vertCount);
  auto* polys = reinterpret_cast<dtPoly*>(cursor);

  uint32_t repaired = 0;
  for (int i = 0; i < header->polyCount; ++i) {
    if (polys[i].flags == 0 && polys[i].getType() == DT_POLYTYPE_GROUND) {
      polys[i].flags = 0x01;
      ++repaired;
    }
  }
  return repaired;
}

void WowToDetour(float x, float y, float z, float out[3]) {
  out[0] = x;
  out[1] = z;
  out[2] = y;
}

Vec3 DetourToWow(float const* pos) {
  return Vec3{pos[0], pos[2], pos[1]};
}

bool FindGroundPoly(dtNavMeshQuery* navQuery, float const queryPos[3],
                    dtQueryFilter const& filter, float maxVerticalSearch,
                    dtPolyRef& outRef, float outNearest[3]) {
  constexpr int kMaxCandidates = 64;
  float const halfExtents[3] = {2.0f, maxVerticalSearch, 2.0f};
  dtPolyRef polys[kMaxCandidates];
  int polyCount = 0;
  dtStatus status = navQuery->queryPolygons(queryPos, halfExtents, &filter,
                                            polys, &polyCount, kMaxCandidates);
  if (dtStatusFailed(status) || polyCount == 0)
    return false;

  dtPolyRef bestRef = 0;
  float bestNearest[3]{};
  float bestScore = std::numeric_limits<float>::infinity();
  bool foundBelow = false;

  for (int i = 0; i < polyCount; ++i) {
    float polyHeight = 0.0f;
    if (dtStatusFailed(navQuery->getPolyHeight(polys[i], queryPos, &polyHeight)))
      continue;

    float nearest[3]{};
    status = navQuery->closestPointOnPoly(polys[i], queryPos, nearest, nullptr);
    if (dtStatusFailed(status))
      continue;

    float const horizontalDx = nearest[0] - queryPos[0];
    float const horizontalDz = nearest[2] - queryPos[2];
    float const horizontalScore =
        horizontalDx * horizontalDx + horizontalDz * horizontalDz;
    bool const below = polyHeight <= queryPos[1] + 2.0f;
    float const verticalScore = std::abs(polyHeight - queryPos[1]);
    float const score = horizontalScore * 25.0f + verticalScore;

    if ((below && !foundBelow) ||
        (below == foundBelow && score < bestScore)) {
      bestRef = polys[i];
      bestNearest[0] = nearest[0];
      bestNearest[1] = polyHeight;
      bestNearest[2] = nearest[2];
      bestScore = score;
      foundBelow = below;
    }
  }

  if (bestRef == 0)
    return false;

  outRef = bestRef;
  outNearest[0] = bestNearest[0];
  outNearest[1] = bestNearest[1];
  outNearest[2] = bestNearest[2];
  return true;
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

namespace {
// Path to the extractor's .map file for (mapId, tileX, tileY). El extractor nombra
// {map}{gx}{gy} con gx = 32 - worldX/533 = 63 - tileX y gy = 32 - worldY/533 = 63 - tileY
// (mismo mapeo que usa el generador en MmapGenerator::MapTilePath). Ambos ejes espejados.
std::filesystem::path MapFilePath(std::string const& dataRoot, uint32_t mapId,
                                  uint32_t tileX, uint32_t tileY) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%03u%02u%02u.map", mapId, 63u - tileX, 63u - tileY);
  return std::filesystem::path(dataRoot) / "maps" / buf;
}

bool MapTileExistsOnDisk(std::string const& dataRoot, uint32_t mapId,
                         uint32_t tileX, uint32_t tileY) {
  std::error_code ec;
  return std::filesystem::exists(
      MapFilePath(dataRoot, mapId, tileX, tileY), ec);
}
} // namespace

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
  if (!file) {
    // Only complain when the source .map exists. If there's no .map either,
    // this tile is genuinely off-continent (ocean / void) and the generator
    // was never expected to produce an .mmtile for it.
    if (MapTileExistsOnDisk(_dataRoot, mapId, tileX, tileY)) {
      LOG_MMAP_WARN(
          "MMAP tile missing but .map present (generator likely failed for "
          "this tile): mapId={} tileX={} tileY={} path={}",
          mapId, tileX, tileY, fileName);
    }
    return false;
  }

  fseek(file, 0, SEEK_END);
  long fileSize = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (fileSize < static_cast<long>(sizeof(MmapTileHeader) + sizeof(dtMeshHeader))) {
    LOG_MMAP_ERROR("MMAP tile too small: mapId={} tileX={} tileY={} path={} size={}",
              mapId, tileX, tileY, fileName, fileSize);
    fclose(file);
    return false;
  }

  std::vector<unsigned char> data(fileSize);
  size_t readSize = fread(data.data(), 1, fileSize, file);
  fclose(file);

  if (readSize != static_cast<size_t>(fileSize))
  {
    LOG_MMAP_ERROR("MMAP tile short read: mapId={} tileX={} tileY={} path={} read={} size={}",
              mapId, tileX, tileY, fileName, readSize, fileSize);
    return false;
  }

  MmapTileHeader const* mmapHeader =
      reinterpret_cast<MmapTileHeader const*>(data.data());
  if (mmapHeader->mmapMagic != kMmapMagic ||
      mmapHeader->dtVersion != DT_NAVMESH_VERSION ||
      mmapHeader->mmapSize == 0 ||
      sizeof(MmapTileHeader) + mmapHeader->mmapSize > data.size()) {
    LOG_MMAP_ERROR("MMAP tile header invalid: mapId={} tileX={} tileY={} path={} magic={} version={} mmapSize={} dataSize={}",
              mapId, tileX, tileY, fileName, mmapHeader->mmapMagic,
              mmapHeader->dtVersion, mmapHeader->mmapSize, data.size());
    return false;
  }

  unsigned char const* tilePayload = data.data() + sizeof(MmapTileHeader);
  dtMeshHeader const* header =
      reinterpret_cast<dtMeshHeader const*>(tilePayload);
  if (header->magic != DT_NAVMESH_MAGIC || header->version != DT_NAVMESH_VERSION) {
    LOG_MMAP_ERROR("MMAP tile nav header invalid: mapId={} tileX={} tileY={} path={} magic={} version={}",
              mapId, tileX, tileY, fileName, header->magic, header->version);
    return false;
  }

  auto* tileData =
      static_cast<unsigned char*>(dtAlloc(mmapHeader->mmapSize, DT_ALLOC_PERM));
  if (!tileData) {
    LOG_MMAP_ERROR("MMAP tile alloc failed: mapId={} tileX={} tileY={} size={}",
              mapId, tileX, tileY, mmapHeader->mmapSize);
    return false;
  }
  std::memcpy(tileData, tilePayload, mmapHeader->mmapSize);
  uint32_t const repairedFlags = RepairMissingPolyFlags(tileData);
  if (repairedFlags != 0) {
    LOG_MMAP_DEBUG("MMAP tile repaired missing poly flags: mapId={} tileX={} tileY={} repaired={}",
              mapId, tileX, tileY, repairedFlags);
  }

  dtStatus status = navMesh->addTile(tileData, static_cast<int>(mmapHeader->mmapSize),
                                     DT_TILE_FREE_DATA, 0, nullptr);
  if (dtStatusFailed(status)) {
    LOG_MMAP_ERROR("MMAP tile add failed: mapId={} tileX={} tileY={} status=0x{:x}",
              mapId, tileX, tileY, static_cast<unsigned int>(status));
    dtFree(tileData);
  }
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
  // Detour's poly ref packs tileBits + polyBits + saltBits == 32 with
  // saltBits >= 10. With maxTiles=1024 (tileBits=10) we can raise maxPolys to
  // 4096 (polyBits=12). A continent's worst case is ~900 ADTs, so 1024 slots
  // is enough and gives addTile much more room before DT_INVALID_PARAM fires
  // on dense terrain.
  params.maxTiles = 1024;
  params.maxPolys = 4096;

  LOG_MMAP_DEBUG("MMAP navmesh init params: mapId={} origin=({}, {}, {}) tileSize={} maxTiles={} maxPolys={}",
            mapId, params.orig[0], params.orig[1], params.orig[2], params.tileWidth,
            params.maxTiles, params.maxPolys);

  dtNavMesh* navMesh = dtAllocNavMesh();
  if (!navMesh)
    return false;

  dtStatus status = navMesh->init(&params);
  if (dtStatusFailed(status)) {
    LOG_MMAP_ERROR("MMAP navmesh init failed: mapId={} status=0x{:x}", mapId,
              static_cast<unsigned int>(status));
    if (dtStatusDetail(status, DT_INVALID_PARAM)) {
      LOG_MMAP_ERROR("MMAP navmesh init detail: invalid params for mapId={} maxTiles={} maxPolys={}",
                mapId, params.maxTiles, params.maxPolys);
    }
    dtFreeNavMesh(navMesh);
    return false;
  }

  bool anyTileLoaded = false;
  uint32_t expectedTiles = 0;
  MapNavMesh loadedEntry;
  loadedEntry.navMesh = navMesh;
  for (uint32_t ty = 0; ty < kTileCountPerAxis; ++ty) {
    for (uint32_t tx = 0; tx < kTileCountPerAxis; ++tx) {
      bool const hasMap = MapTileExistsOnDisk(_dataRoot, mapId, tx, ty);
      if (hasMap)
        ++expectedTiles;
      if (ReadMmapTile(mapId, tx, ty, navMesh)) {
        anyTileLoaded = true;
        loadedEntry.loadedTiles.emplace_back(tx, ty);
      }
    }
  }

  LOG_MMAP_INFO(
      "MMAP navmesh tile load summary: mapId={} loaded={} expected(.map)={} "
      "missing={}",
      mapId, loadedEntry.loadedTiles.size(), expectedTiles,
      expectedTiles > loadedEntry.loadedTiles.size()
          ? expectedTiles - static_cast<uint32_t>(loadedEntry.loadedTiles.size())
          : 0u);

  if (!anyTileLoaded) {
    LOG_MMAP_WARN("MMAP navmesh load skipped: no tiles found for mapId={}", mapId);
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
    LOG_MMAP_ERROR("MMAP navmesh query init failed: mapId={} status=0x{:x}", mapId,
              static_cast<unsigned int>(status));
    dtFreeNavMeshQuery(navQuery);
    dtFreeNavMesh(navMesh);
    return false;
  }

  loadedEntry.navQuery = navQuery;
  _loadedMaps[mapId] = std::move(loadedEntry);
  return true;
}

uint32_t DetourNavMeshManager::GetLoadedMapCount() const {
  return static_cast<uint32_t>(_loadedMaps.size());
}

uint32_t DetourNavMeshManager::GetLoadedTileCount() const {
  uint32_t total = 0;
  for (auto const& [mapId, entry] : _loadedMaps) {
    (void)mapId;
    total += static_cast<uint32_t>(entry.loadedTiles.size());
  }
  return total;
}

std::vector<std::pair<uint32_t, uint32_t>> DetourNavMeshManager::GetLoadedTiles(
    uint32_t mapId) const {
  auto it = _loadedMaps.find(mapId);
  if (it == _loadedMaps.end())
    return {};
  return it->second.loadedTiles;
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

bool DetourNavMeshManager::GetNavMeshHeight(uint32_t mapId, float x, float y,
                                             float zHint, float& outZ) const {
  auto it = _loadedMaps.find(mapId);
  if (it == _loadedMaps.end() || it->second.navQuery == nullptr)
    return false;

  float queryPos[3]{};
  WowToDetour(x, y, zHint, queryPos);

  // We want the floor *below the same XY*, not the closest poly in 3D space.
  // findNearestPoly would happily pick a nearby cliff peak instead of the
  // ground straight under the query. Enumerate polys whose XY footprint
  // covers the query column (tight horizontal extent), then evaluate each
  // poly's actual surface height at (x, y).
  dtQueryFilter filter;
  filter.setIncludeFlags(0xFFFF);
  filter.setExcludeFlags(0);

  constexpr int kMaxCandidates = 32;
  // Slightly relaxed horizontal so we still find a poly when the query lands
  // exactly on a tile/poly boundary.
  float const halfExtents[3] = {1.5f, 1000.0f, 1.5f};
  dtPolyRef polys[kMaxCandidates];
  int polyCount = 0;
  dtStatus status = it->second.navQuery->queryPolygons(queryPos, halfExtents,
                                                       &filter, polys,
                                                       &polyCount,
                                                       kMaxCandidates);
  if (dtStatusFailed(status) || polyCount == 0)
    return false;

  // Slop accounts for floating point error and small ledges; anything above
  // the query Y by more than this is treated as a ceiling we don't want.
  constexpr float kAboveSlop = 2.0f;

  float bestBelow = -std::numeric_limits<float>::infinity();
  float bestAbove = std::numeric_limits<float>::infinity();
  bool hasBelow = false;
  bool hasAbove = false;

  for (int i = 0; i < polyCount; ++i) {
    float h = 0.0f;
    if (dtStatusFailed(
            it->second.navQuery->getPolyHeight(polys[i], queryPos, &h)))
      continue;

    if (h <= queryPos[1] + kAboveSlop) {
      if (!hasBelow || h > bestBelow) {
        bestBelow = h;
        hasBelow = true;
      }
    } else if (!hasAbove || h < bestAbove) {
      bestAbove = h;
      hasAbove = true;
    }
  }

  if (hasBelow) {
    outZ = bestBelow;
    return true;
  }
  if (hasAbove) {
    outZ = bestAbove;
    return true;
  }
  return false;
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

  bool const foundStart =
      FindGroundPoly(navQuery, startPos, filter, _config.maxSearchRadius,
                     startRef, startNearest);
  if (!foundStart || startRef == 0) {
    LOG_MMAP_DEBUG("MMAP path start not found: mapId={} start=({}, {}, {}) status=0x{:x}",
              req.mapId, req.startX, req.startY, req.startZ, 0u);
    result.status = FindPathStatus::NoPath;
    return result;
  }

  bool const foundEnd =
      FindGroundPoly(navQuery, endPos, filter, _config.maxSearchRadius,
                     endRef, endNearest);
  if (!foundEnd || endRef == 0) {
    LOG_MMAP_DEBUG("MMAP path end not found: mapId={} end=({}, {}, {}) status=0x{:x}",
              req.mapId, req.endX, req.endY, req.endZ, 0u);
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
  dtStatus status = navQuery->findPath(startRef, endRef, startNearest,
                                       endNearest, &filter, pathPolys,
                                       &pathCount, maxPathPolys);
  if (dtStatusFailed(status) || pathCount == 0) {
    LOG_MMAP_DEBUG("MMAP path failed: mapId={} pathCount={} status=0x{:x}", req.mapId,
              pathCount, static_cast<unsigned int>(status));
    result.status = FindPathStatus::NoPath;
    return result;
  }

  status = navQuery->findStraightPath(
      startNearest, endNearest, pathPolys, pathCount, straightPath,
      straightPathFlags, straightPathPolys, &straightPathCount,
      maxPathPolys, 0);
  if (dtStatusFailed(status) || straightPathCount == 0) {
    LOG_MMAP_DEBUG("MMAP straight path failed: mapId={} straightCount={} status=0x{:x}",
              req.mapId, straightPathCount, static_cast<unsigned int>(status));
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

  // findStraightPath solo devuelve las esquinas del camino, con tramos largos en
  // terreno abierto. Los partimos en pasos cortos para que la criatura camine bien
  // (siguiendo de cerca el corredor) y para que los marcadores de .mmap path queden
  // repartidos a lo largo del camino. La Z se resuelve aparte: la criatura la pega
  // al suelo cada tick y los marcadores via GetHeight.
  if (result.waypoints.size() >= 2) {
    constexpr float kSegmentStep = 5.0f;  // tramos cortos (yardas)
    constexpr size_t kMaxWaypoints = 256;
    std::vector<Vec3> dense;
    dense.reserve(std::min<size_t>(kMaxWaypoints, result.waypoints.size() * 8 + 1));
    for (size_t i = 0; i + 1 < result.waypoints.size() &&
                       dense.size() < kMaxWaypoints; ++i) {
      Vec3 const a = result.waypoints[i];
      Vec3 const b = result.waypoints[i + 1];
      float const dx = b.x - a.x, dy = b.y - a.y;
      float const segLen = std::sqrt(dx * dx + dy * dy);
      int const steps = std::max(1, static_cast<int>(segLen / kSegmentStep));
      for (int s = 0; s < steps && dense.size() < kMaxWaypoints; ++s) {
        float const t = static_cast<float>(s) / static_cast<float>(steps);
        dense.push_back(
            Vec3{a.x + dx * t, a.y + dy * t, a.z + (b.z - a.z) * t});
      }
    }
    dense.push_back(result.waypoints.back());
    result.waypoints = std::move(dense);
  }

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
