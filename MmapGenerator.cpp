#include "MmapGenerator.h"

#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourCommon.h>

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

float TileOriginX(uint32_t tileX) {
  return kMapOrigin + static_cast<float>(tileX) * kTileSize;
}

float TileOriginY(uint32_t tileY) {
  return kMapOrigin + static_cast<float>(tileY) * kTileSize;
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

} // namespace

MmapGenerator::MmapGenerator(MmapGeneratorConfig config)
    : _config(std::move(config)) {}

bool MmapGenerator::LoadTerrainData(uint32_t tileX, uint32_t tileY,
                                     TileTerrainData& out) const {
  std::ostringstream ss;
  ss << std::setfill('0') << std::setw(3) << _config.mapId
     << std::setw(2) << tileY << std::setw(2) << tileX << ".map";
  std::string const fileName =
      (std::filesystem::path(_config.mapsDir) / ss.str()).string();

  FILE* file = fopen(fileName.c_str(), "rb");
  if (!file)
    return false;

  uint32_t mapMagic = 0;
  if (fread(&mapMagic, 4, 1, file) != 1 || mapMagic != 0x5350414Du) {
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

  constexpr int kGridSize = 128;
  out.width = kGridSize;
  out.height = kGridSize;
  out.cellWidth = kTileSize / static_cast<float>(kGridSize);
  out.cellHeight = kTileSize / static_cast<float>(kGridSize);
  out.minX = TileOriginX(tileX);
  out.minY = TileOriginY(tileY);
  out.heights.resize(kGridSize * kGridSize);

  int const count = kGridSize * kGridSize;
  if (heightFlags & 1) {
    std::fill(out.heights.begin(), out.heights.end(), gridHeight);
  } else if (heightFlags & 2) {
    for (int i = 0; i < count; ++i) {
      int16_t v = 0; fread(&v, 2, 1, file);
      out.heights[i] = gridHeight + static_cast<float>(v);
    }
  } else if (heightFlags & 4) {
    for (int i = 0; i < count; ++i) {
      int8_t v = 0; fread(&v, 1, 1, file);
      out.heights[i] = gridHeight + static_cast<float>(v);
    }
  } else {
    for (int i = 0; i < count; ++i) {
      float h = 0.0f; fread(&h, 4, 1, file);
      out.heights[i] = h;
    }
  }

  fclose(file);
  return true;
}

bool MmapGenerator::BuildTileNavMesh(TileTerrainData const& terrain,
                                      uint32_t tileX, uint32_t tileY,
                                      std::string const& outputPath) const {
  float const bmin[3] = {terrain.minX, terrain.minY, -500.0f};
  float const bmax[3] = {terrain.minX + kTileSize, terrain.minY + kTileSize, 500.0f};

  float const cellSize = std::max(1.5f, _config.cellSize);
  float const cellHeight = std::max(0.3f, _config.cellHeight);
  int const tileW = static_cast<int>(kTileSize / cellSize + 0.5f);
  int const tileH = static_cast<int>(kTileSize / cellSize + 0.5f);

  rcContext ctx;

  rcHeightfield* solid = rcAllocHeightfield();
  if (!solid) { printf("A"); return false; }
  if (!rcCreateHeightfield(&ctx, *solid, tileW, tileH, bmin, bmax, cellSize, cellHeight)) {
    printf("B"); rcFreeHeightField(solid); return false;
  }

  // Downsample terrain
  std::vector<float> hfVerts((tileW + 1) * (tileH + 1) * 3);
  std::vector<int> hfTris(tileW * tileH * 6);
  std::vector<unsigned char> triAreas(tileW * tileH * 2, 0);

  int vi = 0;
  for (int ty = 0; ty <= tileH; ++ty) {
    for (int tx = 0; tx <= tileW; ++tx) {
      float wx = terrain.minX + static_cast<float>(tx) * cellSize;
      float wy = terrain.minY + static_cast<float>(ty) * cellSize;
      int sx = std::min(static_cast<int>(tx * cellSize / terrain.cellWidth), terrain.width - 1);
      int sy = std::min(static_cast<int>(ty * cellSize / terrain.cellHeight), terrain.height - 1);
      hfVerts[vi++] = wx;
      hfVerts[vi++] = wy;
      hfVerts[vi++] = terrain.heights[sy * terrain.width + sx];
    }
  }

  int ti = 0;
  for (int ty = 0; ty < tileH; ++ty) {
    for (int tx = 0; tx < tileW; ++tx) {
      int a = ty * (tileW + 1) + tx;
      int b = a + 1;
      int c = a + tileW + 1;
      int d = c + 1;
      hfTris[ti++] = a; hfTris[ti++] = c; hfTris[ti++] = b;
      hfTris[ti++] = b; hfTris[ti++] = c; hfTris[ti++] = d;
    }
  }

  rcRasterizeTriangles(&ctx, hfVerts.data(), (tileW + 1) * (tileH + 1),
                       hfTris.data(), triAreas.data(), tileW * tileH * 2, *solid, 0);

  int const walkableClimb = std::max(1, static_cast<int>(_config.agentMaxClimb / cellHeight));
  int const walkableHeight = std::max(1, static_cast<int>(_config.agentHeight / cellHeight));

  rcCompactHeightfield* chf = rcAllocCompactHeightfield();
  if (!chf) { printf("C"); rcFreeHeightField(solid); return false; }
  if (!rcBuildCompactHeightfield(&ctx, walkableClimb, walkableHeight, *solid, *chf)) {
    printf("D"); rcFreeCompactHeightfield(chf); rcFreeHeightField(solid); return false;
  }
  rcFreeHeightField(solid);

  printf(" spans=%d ", chf->spanCount); fflush(stdout);
  if (chf->spanCount == 0) { printf("Q"); rcFreeCompactHeightfield(chf); return false; }

  int const erosionRadius = std::max(0, static_cast<int>(_config.agentRadius / cellSize));
  if (!rcErodeWalkableArea(&ctx, erosionRadius, *chf)) {
    printf("E"); rcFreeCompactHeightfield(chf); return false;
  }

  if (!rcBuildRegionsMonotone(&ctx, *chf, 0, _config.minRegionArea, _config.mergeRegionArea)) {
    printf("F"); rcFreeCompactHeightfield(chf); return false;
  }

  rcContourSet* cset = rcAllocContourSet();
  if (!cset) { printf("G"); rcFreeCompactHeightfield(chf); return false; }
  if (!rcBuildContours(&ctx, *chf, _config.maxSimplificationError, _config.maxEdgeLen, *cset)) {
    printf("H"); rcFreeContourSet(cset); rcFreeCompactHeightfield(chf); return false;
  }

  rcPolyMesh* pmesh = rcAllocPolyMesh();
  if (!pmesh) { printf("I"); rcFreeContourSet(cset); rcFreeCompactHeightfield(chf); return false; }
  if (!rcBuildPolyMesh(&ctx, *cset, _config.maxVertsPerPoly, *pmesh)) {
    printf("J"); rcFreePolyMesh(pmesh); rcFreeContourSet(cset); rcFreeCompactHeightfield(chf); return false;
  }

  rcPolyMeshDetail* dmesh = rcAllocPolyMeshDetail();
  if (!dmesh) { printf("K"); rcFreePolyMesh(pmesh); rcFreeContourSet(cset); rcFreeCompactHeightfield(chf); return false; }
  if (!rcBuildPolyMeshDetail(&ctx, *pmesh, *chf, _config.detailSampleDist,
                              _config.detailSampleMaxError, *dmesh)) {
    printf("L"); rcFreePolyMeshDetail(dmesh); rcFreePolyMesh(pmesh); rcFreeContourSet(cset); rcFreeCompactHeightfield(chf); return false;
  }
  rcFreeCompactHeightfield(chf);
  rcFreeContourSet(cset);

  for (int i = 0; i < pmesh->npolys; ++i) {
    if (pmesh->areas[i] == RC_WALKABLE_AREA)
      pmesh->flags[i] = 0x01;
  }

  if (pmesh->npolys == 0) {
    printf("Z"); rcFreePolyMeshDetail(dmesh); rcFreePolyMesh(pmesh); return false;
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
  rcVcopy(params.bmin, pmesh->bmin);
  rcVcopy(params.bmax, pmesh->bmax);
  params.cs = cellSize;
  params.ch = cellHeight;
  params.buildBvTree = true;

  unsigned char* navData = nullptr;
  int navDataSize = 0;
  if (!dtCreateNavMeshData(&params, &navData, &navDataSize)) {
    printf("M"); rcFreePolyMeshDetail(dmesh); rcFreePolyMesh(pmesh); return false;
  }

  dtNavMesh* tileNavMesh = dtAllocNavMesh();
  if (!tileNavMesh) {
    printf("N"); dtFree(navData); rcFreePolyMeshDetail(dmesh); rcFreePolyMesh(pmesh); return false;
  }

  dtNavMeshParams navParams{};
  rcVcopy(navParams.orig, bmin);
  navParams.tileWidth = kTileSize;
  navParams.tileHeight = kTileSize;
  navParams.maxTiles = 1;
  navParams.maxPolys = 1 << 16;

  dtStatus dtStatusFlags = tileNavMesh->init(&navParams);
  if (dtStatusFailed(dtStatusFlags)) {
    printf("O"); dtFreeNavMesh(tileNavMesh); dtFree(navData); rcFreePolyMeshDetail(dmesh); rcFreePolyMesh(pmesh); return false;
  }

  dtStatusFlags = tileNavMesh->addTile(navData, navDataSize, DT_TILE_FREE_DATA, 0, nullptr);
  if (dtStatusFailed(dtStatusFlags)) {
    printf("P"); dtFreeNavMesh(tileNavMesh); rcFreePolyMeshDetail(dmesh); rcFreePolyMesh(pmesh); return false;
  }

  SaveNavMeshTile(tileNavMesh, _config.mapId, tileX, tileY, outputPath);

  dtFreeNavMesh(tileNavMesh);
  rcFreePolyMeshDetail(dmesh);
  rcFreePolyMesh(pmesh);
  return true;
}

bool MmapGenerator::Generate(uint32_t tileX, uint32_t tileY) {
  TileTerrainData terrain;
  if (!LoadTerrainData(tileX, tileY, terrain)) {
    // Fallback: use flat terrain for testing
    terrain.width = 64;
    terrain.height = 64;
    terrain.cellWidth = kTileSize / 64.0f;
    terrain.cellHeight = kTileSize / 64.0f;
    terrain.minX = TileOriginX(tileX);
    terrain.minY = TileOriginY(tileY);
    terrain.heights.assign(64 * 64, 0.0f);
  }

  std::filesystem::create_directories(_config.mmapsDir);
  return BuildTileNavMesh(terrain, tileX, tileY, _config.mmapsDir);
}

bool MmapGenerator::GenerateAllTiles() {
  bool anySuccess = false;
  for (uint32_t tileY = 0; tileY < 64; ++tileY) {
    for (uint32_t tileX = 0; tileX < 64; ++tileX) {
      if (Generate(tileX, tileY)) {
        anySuccess = true;
        printf("."); fflush(stdout);
      }
    }
  }
  if (anySuccess) printf("\n");
  return anySuccess;
}

} // namespace Firelands
