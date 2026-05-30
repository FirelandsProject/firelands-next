#include "MmapGenerator.h"

#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourCommon.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <vector>

namespace Firelands {

namespace {

constexpr float kTileSize = 533.33333f;
constexpr float kMapOrigin = -17066.66656f;
constexpr uint32_t kMapMagic = 0x5350414Du;        // 'MAPS'
constexpr uint32_t kMapHeightMagic = 0x54474D48u;  // 'MHGT'
constexpr uint32_t kMapHeightNoHeight = 0x0001;
constexpr uint32_t kMapHeightAsInt16 = 0x0002;
constexpr uint32_t kMapHeightAsInt8 = 0x0004;
constexpr int kTerrainGridSize = 128;
constexpr int kTerrainVertexCount = kTerrainGridSize + 1;

float TileOriginX(uint32_t tileX) {
  return kMapOrigin + static_cast<float>(tileX) * kTileSize;
}

float TileOriginY(uint32_t tileY) {
  return kMapOrigin + static_cast<float>(tileY) * kTileSize;
}

std::filesystem::path MapTilePath(std::string const& mapsDir, uint32_t mapId,
                                  uint32_t tileX, uint32_t tileY) {
  std::ostringstream ss;
  ss << std::setfill('0') << std::setw(3) << mapId
     << std::setw(2) << tileY << std::setw(2) << tileX << ".map";
  return std::filesystem::path(mapsDir) / ss.str();
}

struct MmapTileHeader {
  uint32_t mmapMagic;
  uint32_t dtVersion;
  uint32_t mmapSize;
  unsigned char usesLiquids;
  unsigned char padding[3];
};

void SaveNavMeshTile(dtNavMesh const* navMesh, uint32_t mapId,
                     uint32_t tileX, uint32_t tileY,
                     std::string const& outputPath) {
  dtMeshTile const* tile = navMesh->getTile(0);
  if (!tile || !tile->data || tile->dataSize == 0)
    return;

  MmapTileHeader header{};
  header.mmapMagic = 'M' | ('M' << 8) | ('A' << 16) | ('P' << 24);
  header.dtVersion = DT_NAVMESH_VERSION;
  header.mmapSize = static_cast<uint32_t>(tile->dataSize);
  header.usesLiquids = 0;

  std::string fileName = outputPath + "/" +
                          std::to_string(mapId) + "_" +
                          std::to_string(tileX) + "_" +
                          std::to_string(tileY) + ".mmtile";

  FILE* file = fopen(fileName.c_str(), "wb");
  if (!file)
    return;

  fwrite(&header, sizeof(header), 1, file);
  fwrite(tile->data, tile->dataSize, 1, file);
  fclose(file);
}

void PrintTileProgress(uint32_t tileX, uint32_t tileY, int percent,
                       char const* stage) {
  printf("\r  tile (%02u,%02u) [%3d%%] %-28s", tileX, tileY, percent, stage);
  fflush(stdout);
}

void PrintTileFailure(uint32_t tileX, uint32_t tileY, char const* stage) {
  printf("\r  tile (%02u,%02u) [FAIL] %s\n", tileX, tileY, stage);
  fflush(stdout);
}

} // namespace

MmapGenerator::MmapGenerator(MmapGeneratorConfig config)
    : _config(std::move(config)) {}

bool MmapGenerator::LoadTerrainData(uint32_t tileX, uint32_t tileY,
                                     TileTerrainData& out) const {
  std::string const fileName =
      MapTilePath(_config.mapsDir, _config.mapId, tileX, tileY).string();

  FILE* file = fopen(fileName.c_str(), "rb");
  if (!file)
    return false;

  uint32_t mapMagic = 0;
  if (fread(&mapMagic, 4, 1, file) != 1 || mapMagic != kMapMagic) {
    fclose(file);
    return false;
  }

  uint32_t versionMagic = 0, buildMagic = 0;
  uint32_t areaMapOffset = 0, areaMapSize = 0;
  uint32_t heightMapOffset = 0, heightMapSize = 0;
  uint32_t liquidMapOffset = 0, liquidMapSize = 0;
  uint32_t holesOffset = 0, holesSize = 0;
  fread(&versionMagic, 4, 1, file);
  fread(&buildMagic, 4, 1, file);
  fread(&areaMapOffset, 4, 1, file);
  fread(&areaMapSize, 4, 1, file);
  fread(&heightMapOffset, 4, 1, file);
  fread(&heightMapSize, 4, 1, file);
  fread(&liquidMapOffset, 4, 1, file);
  fread(&liquidMapSize, 4, 1, file);
  fread(&holesOffset, 4, 1, file);
  fread(&holesSize, 4, 1, file);
  (void)versionMagic; (void)buildMagic;
  (void)areaMapOffset; (void)areaMapSize;
  (void)liquidMapOffset; (void)liquidMapSize;
  (void)holesOffset; (void)holesSize;

  fseek(file, static_cast<long>(heightMapOffset), SEEK_SET);

  uint32_t heightFourcc = 0, heightFlags = 0;
  float gridHeight = 0.0f, gridMaxHeight = 0.0f;
  fread(&heightFourcc, 4, 1, file);
  fread(&heightFlags, 4, 1, file);
  fread(&gridHeight, 4, 1, file);
  fread(&gridMaxHeight, 4, 1, file);

  if (heightFourcc != kMapHeightMagic) {
    fclose(file);
    return false;
  }

  out.width = kTerrainVertexCount;
  out.height = kTerrainVertexCount;
  out.cellSize = kTileSize / static_cast<float>(kTerrainGridSize);
  out.minX = TileOriginX(tileX);
  out.minY = TileOriginY(tileY);
  out.minZ = gridHeight;
  out.maxZ = gridMaxHeight;
  out.heights.resize(kTerrainVertexCount * kTerrainVertexCount);

  int const count = kTerrainVertexCount * kTerrainVertexCount;
  float const heightRange = std::max(0.0f, gridMaxHeight - gridHeight);
  if (heightFlags & kMapHeightNoHeight) {
    std::fill(out.heights.begin(), out.heights.end(), gridHeight);
  } else if (heightFlags & kMapHeightAsInt16) {
    float const invStep = heightRange > 0.0f ? heightRange / 65535.0f : 0.0f;
    for (int i = 0; i < count; ++i) {
      uint16_t v = 0;
      if (fread(&v, sizeof(v), 1, file) != 1) {
        fclose(file);
        return false;
      }
      out.heights[i] = gridHeight + static_cast<float>(v) * invStep;
    }
  } else if (heightFlags & kMapHeightAsInt8) {
    float const invStep = heightRange > 0.0f ? heightRange / 255.0f : 0.0f;
    for (int i = 0; i < count; ++i) {
      uint8_t v = 0;
      if (fread(&v, sizeof(v), 1, file) != 1) {
        fclose(file);
        return false;
      }
      out.heights[i] = gridHeight + static_cast<float>(v) * invStep;
    }
  } else {
    for (int i = 0; i < count; ++i) {
      float h = 0.0f;
      if (fread(&h, sizeof(h), 1, file) != 1) {
        fclose(file);
        return false;
      }
      out.heights[i] = h;
    }
  }

  fclose(file);
  return true;
}

bool MmapGenerator::BuildTileNavMesh(TileTerrainData const& terrain,
                                      uint32_t tileX, uint32_t tileY,
                                      std::string const& outputPath) const {
  float const minZ = terrain.minZ - _config.agentHeight - 5.0f;
  float const maxZ = terrain.maxZ + _config.agentHeight + 5.0f;
  float const bmin[3] = {terrain.minX, minZ, terrain.minY};
  float const bmax[3] = {terrain.minX + kTileSize, maxZ, terrain.minY + kTileSize};

  float const cellSize = std::max(1.5f, _config.cellSize);
  float const cellHeight = std::max(0.3f, _config.cellHeight);
  int const tileW = static_cast<int>(kTileSize / cellSize + 0.5f);
  int const tileH = static_cast<int>(kTileSize / cellSize + 0.5f);

  rcContext ctx;

  rcHeightfield* solid = rcAllocHeightfield();
  if (!solid) {
    PrintTileFailure(tileX, tileY, "could not allocate heightfield");
    return false;
  }
  if (!rcCreateHeightfield(&ctx, *solid, tileW, tileH, bmin, bmax, cellSize, cellHeight)) {
    PrintTileFailure(tileX, tileY, "could not create heightfield");
    rcFreeHeightField(solid);
    return false;
  }
  PrintTileProgress(tileX, tileY, 15, "heightfield ready");

  int const walkableClimb = std::max(1, static_cast<int>(_config.agentMaxClimb / cellHeight));
  int const walkableHeight = std::max(1, static_cast<int>(_config.agentHeight / cellHeight));

  std::vector<float> verts;
  verts.reserve(static_cast<size_t>(terrain.width * terrain.height * 3));
  for (int y = 0; y < terrain.height; ++y) {
    for (int x = 0; x < terrain.width; ++x) {
      float const wx = terrain.minX + static_cast<float>(x) * terrain.cellSize;
      float const wy = terrain.minY + static_cast<float>(y) * terrain.cellSize;
      float const wz = terrain.heights[static_cast<size_t>(y * terrain.width + x)];
      verts.push_back(wx);
      verts.push_back(wz);
      verts.push_back(wy);
    }
  }

  std::vector<int> tris;
  tris.reserve(static_cast<size_t>(kTerrainGridSize * kTerrainGridSize * 6));
  for (int y = 0; y < terrain.height - 1; ++y) {
    for (int x = 0; x < terrain.width - 1; ++x) {
      int const a = y * terrain.width + x;
      int const b = y * terrain.width + x + 1;
      int const c = (y + 1) * terrain.width + x;
      int const d = (y + 1) * terrain.width + x + 1;
      tris.push_back(a); tris.push_back(c); tris.push_back(b);
      tris.push_back(b); tris.push_back(c); tris.push_back(d);
    }
  }

  int const triCount = static_cast<int>(tris.size() / 3);
  std::vector<unsigned char> triAreas(static_cast<size_t>(triCount), RC_WALKABLE_AREA);
  rcRasterizeTriangles(&ctx, verts.data(), static_cast<int>(verts.size() / 3),
                       tris.data(), triAreas.data(), triCount, *solid,
                       walkableClimb);
  rcFilterLowHangingWalkableObstacles(&ctx, walkableClimb, *solid);
  rcFilterLedgeSpans(&ctx, walkableHeight, walkableClimb, *solid);
  rcFilterWalkableLowHeightSpans(&ctx, walkableHeight, *solid);
  PrintTileProgress(tileX, tileY, 30, "terrain rasterized");

  rcCompactHeightfield* chf = rcAllocCompactHeightfield();
  if (!chf) {
    PrintTileFailure(tileX, tileY, "could not allocate compact heightfield");
    rcFreeHeightField(solid);
    return false;
  }
  if (!rcBuildCompactHeightfield(&ctx, walkableClimb, walkableHeight, *solid, *chf)) {
    PrintTileFailure(tileX, tileY, "could not compact heightfield");
    rcFreeCompactHeightfield(chf);
    rcFreeHeightField(solid);
    return false;
  }
  rcFreeHeightField(solid);

  if (chf->spanCount == 0) {
    PrintTileFailure(tileX, tileY, "no compact spans generated");
    rcFreeCompactHeightfield(chf);
    return false;
  }
  PrintTileProgress(tileX, tileY, 45, "compact heightfield ready");

  int const erosionRadius = std::max(0, static_cast<int>(_config.agentRadius / cellSize));
  if (!rcErodeWalkableArea(&ctx, erosionRadius, *chf)) {
    PrintTileFailure(tileX, tileY, "could not erode walkable area");
    rcFreeCompactHeightfield(chf);
    return false;
  }
  PrintTileProgress(tileX, tileY, 55, "agent radius applied");

  if (!rcBuildRegionsMonotone(&ctx, *chf, 0, _config.minRegionArea, _config.mergeRegionArea)) {
    PrintTileFailure(tileX, tileY, "could not build regions");
    rcFreeCompactHeightfield(chf);
    return false;
  }
  PrintTileProgress(tileX, tileY, 65, "regions built");

  rcContourSet* cset = rcAllocContourSet();
  if (!cset) {
    PrintTileFailure(tileX, tileY, "could not allocate contours");
    rcFreeCompactHeightfield(chf);
    return false;
  }
  if (!rcBuildContours(&ctx, *chf, _config.maxSimplificationError, _config.maxEdgeLen, *cset)) {
    PrintTileFailure(tileX, tileY, "could not build contours");
    rcFreeContourSet(cset);
    rcFreeCompactHeightfield(chf);
    return false;
  }
  PrintTileProgress(tileX, tileY, 75, "contours built");

  rcPolyMesh* pmesh = rcAllocPolyMesh();
  if (!pmesh) {
    PrintTileFailure(tileX, tileY, "could not allocate poly mesh");
    rcFreeContourSet(cset);
    rcFreeCompactHeightfield(chf);
    return false;
  }
  if (!rcBuildPolyMesh(&ctx, *cset, _config.maxVertsPerPoly, *pmesh)) {
    PrintTileFailure(tileX, tileY, "could not build poly mesh");
    rcFreePolyMesh(pmesh);
    rcFreeContourSet(cset);
    rcFreeCompactHeightfield(chf);
    return false;
  }
  PrintTileProgress(tileX, tileY, 85, "poly mesh built");

  rcPolyMeshDetail* dmesh = rcAllocPolyMeshDetail();
  if (!dmesh) {
    PrintTileFailure(tileX, tileY, "could not allocate detail mesh");
    rcFreePolyMesh(pmesh);
    rcFreeContourSet(cset);
    rcFreeCompactHeightfield(chf);
    return false;
  }
  if (!rcBuildPolyMeshDetail(&ctx, *pmesh, *chf, _config.detailSampleDist,
                              _config.detailSampleMaxError, *dmesh)) {
    PrintTileFailure(tileX, tileY, "could not build detail mesh");
    rcFreePolyMeshDetail(dmesh);
    rcFreePolyMesh(pmesh);
    rcFreeContourSet(cset);
    rcFreeCompactHeightfield(chf);
    return false;
  }
  rcFreeCompactHeightfield(chf);
  rcFreeContourSet(cset);
  PrintTileProgress(tileX, tileY, 92, "detail mesh built");

  for (int i = 0; i < pmesh->npolys; ++i) {
    if (pmesh->areas[i] == RC_WALKABLE_AREA)
      pmesh->flags[i] = 0x01;
  }

  if (pmesh->npolys == 0) {
    PrintTileFailure(tileX, tileY, "no polygons generated");
    rcFreePolyMeshDetail(dmesh);
    rcFreePolyMesh(pmesh);
    return false;
  }

  dtNavMeshCreateParams params{};
  std::memset(&params, 0, sizeof(params));
  params.verts = pmesh->verts;
  params.vertCount = pmesh->nverts;
  params.polys = pmesh->polys;
  params.polyAreas = pmesh->areas;
  params.polyFlags = pmesh->flags;
  params.polyCount = pmesh->npolys;
  params.nvp = pmesh->nvp;
  params.detailMeshes = dmesh->meshes;
  params.detailVerts = dmesh->verts;
  params.detailVertsCount = dmesh->nverts;
  params.detailTris = dmesh->tris;
  params.detailTriCount = dmesh->ntris;
  params.walkableHeight = _config.agentHeight;
  params.walkableRadius = _config.agentRadius;
  params.walkableClimb = _config.agentMaxClimb;
  params.tileX = static_cast<int>(tileX);
  params.tileY = static_cast<int>(tileY);
  params.tileLayer = 0;
  rcVcopy(params.bmin, pmesh->bmin);
  rcVcopy(params.bmax, pmesh->bmax);
  params.cs = cellSize;
  params.ch = cellHeight;
  params.buildBvTree = true;

  unsigned char* navData = nullptr;
  int navDataSize = 0;
  if (!dtCreateNavMeshData(&params, &navData, &navDataSize)) {
    PrintTileFailure(tileX, tileY, "could not create Detour nav data");
    rcFreePolyMeshDetail(dmesh);
    rcFreePolyMesh(pmesh);
    return false;
  }

  dtNavMesh* tileNavMesh = dtAllocNavMesh();
  if (!tileNavMesh) {
    PrintTileFailure(tileX, tileY, "could not allocate Detour nav mesh");
    dtFree(navData);
    rcFreePolyMeshDetail(dmesh);
    rcFreePolyMesh(pmesh);
    return false;
  }

  dtNavMeshParams navParams{};
  rcVcopy(navParams.orig, bmin);
  navParams.tileWidth = kTileSize;
  navParams.tileHeight = kTileSize;
  navParams.maxTiles = 1;
  navParams.maxPolys = 1 << 16;

  dtStatus dtStatusFlags = tileNavMesh->init(&navParams);
  if (dtStatusFailed(dtStatusFlags)) {
    PrintTileFailure(tileX, tileY, "could not initialize Detour nav mesh");
    dtFreeNavMesh(tileNavMesh);
    dtFree(navData);
    rcFreePolyMeshDetail(dmesh);
    rcFreePolyMesh(pmesh);
    return false;
  }

  dtStatusFlags = tileNavMesh->addTile(navData, navDataSize, DT_TILE_FREE_DATA, 0, nullptr);
  if (dtStatusFailed(dtStatusFlags)) {
    PrintTileFailure(tileX, tileY, "could not add Detour tile");
    dtFreeNavMesh(tileNavMesh);
    rcFreePolyMeshDetail(dmesh);
    rcFreePolyMesh(pmesh);
    return false;
  }
  PrintTileProgress(tileX, tileY, 98, "Detour tile ready");

  SaveNavMeshTile(tileNavMesh, _config.mapId, tileX, tileY, outputPath);
  PrintTileProgress(tileX, tileY, 100, "saved");
  printf("\n");

  dtFreeNavMesh(tileNavMesh);
  rcFreePolyMeshDetail(dmesh);
  rcFreePolyMesh(pmesh);
  return true;
}

bool MmapGenerator::Generate(uint32_t tileX, uint32_t tileY) {
  TileTerrainData terrain;
  if (!LoadTerrainData(tileX, tileY, terrain)) {
    PrintTileFailure(tileX, tileY, "terrain .map missing or invalid");
    return false;
  }

  std::filesystem::create_directories(_config.mmapsDir);
  return BuildTileNavMesh(terrain, tileX, tileY, _config.mmapsDir);
}

bool MmapGenerator::GenerateAllTiles() {
  bool anySuccess = false;
  uint32_t processed = 0;
  uint32_t succeeded = 0;
  uint32_t totalTiles = 0;

  for (uint32_t tileY = 0; tileY < 64; ++tileY) {
    for (uint32_t tileX = 0; tileX < 64; ++tileX) {
      if (std::filesystem::exists(
              MapTilePath(_config.mapsDir, _config.mapId, tileX, tileY))) {
        ++totalTiles;
      }
    }
  }

  if (totalTiles == 0) {
    printf("No terrain .map tiles found for map %u in %s\n", _config.mapId,
           _config.mapsDir.c_str());
    return false;
  }

  for (uint32_t tileY = 0; tileY < 64; ++tileY) {
    for (uint32_t tileX = 0; tileX < 64; ++tileX) {
      if (!std::filesystem::exists(
              MapTilePath(_config.mapsDir, _config.mapId, tileX, tileY))) {
        continue;
      }

      ++processed;
      int const percent = static_cast<int>((processed * 100u) / totalTiles);
      printf("\n[%3d%%] tile %4u/%u map %u (%02u,%02u)\n",
             percent, processed, totalTiles, _config.mapId, tileX, tileY);
      if (Generate(tileX, tileY)) {
        anySuccess = true;
        ++succeeded;
      }
    }
  }
  printf("\nDone: %u/%u existing terrain tiles generated.\n", succeeded,
         totalTiles);
  return anySuccess;
}

} // namespace Firelands
