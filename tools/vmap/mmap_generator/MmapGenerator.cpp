#include "MmapGenerator.h"

#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourCommon.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
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

void SaveNavMeshTile(dtNavMesh const* navMesh, uint32_t tileX, uint32_t tileY,
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
                         std::to_string(0) + "_" +
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
  std::string const fileName =
      (std::filesystem::path(_config.mapsDir) /
       (std::to_string(_config.mapId) +
        std::to_string(tileY) +
        std::to_string(tileX) + ".map")).string();

  FILE* file = fopen(fileName.c_str(), "rb");
  if (!file)
    return false;

  char magic[5] = {};
  if (fread(magic, 1, 4, file) != 4 || std::memcmp(magic, "MAPS", 4) != 0) {
    fclose(file);
    return false;
  }

  uint32_t header[3] = {};
  fread(header, sizeof(uint32_t), 3, file);

  out.cellWidth = kTileSize / static_cast<float>(header[1]);
  out.cellHeight = kTileSize / static_cast<float>(header[2]);
  out.width = static_cast<int>(header[1]);
  out.height = static_cast<int>(header[2]);
  out.minX = TileOriginX(tileX);
  out.minY = TileOriginY(tileY);

  out.heights.resize(out.width * out.height);

  for (int y = 0; y < out.height; ++y) {
    for (int x = 0; x < out.width; ++x) {
      float h = 0.0f;
      fread(&h, sizeof(float), 1, file);
      out.heights[y * out.width + x] = h;
    }
  }

  fclose(file);
  return true;
}

bool MmapGenerator::BuildTileNavMesh(TileTerrainData const& terrain,
                                      uint32_t tileX, uint32_t tileY,
                                      std::string const& outputPath) const {
  float const bmin[3] = {terrain.minX, terrain.minY,
                          -500.0f};
  float const bmax[3] = {terrain.minX + kTileSize,
                          terrain.minY + kTileSize, 500.0f};

  int const tileWidth =
      static_cast<int>(kTileSize / _config.cellSize + 0.5f);
  int const tileHeight =
      static_cast<int>(kTileSize / _config.cellSize + 0.5f);

  rcContext ctx;

  rcHeightfield* solid = rcAllocHeightfield();
  if (!solid)
    return false;
  if (!rcCreateHeightfield(&ctx, *solid, tileWidth, tileHeight, bmin, bmax,
                            _config.cellSize, _config.cellHeight)) {
    rcFreeHeightField(solid);
    return false;
  }

  unsigned char* triAreas = new unsigned char[terrain.width * terrain.height * 2];
  std::memset(triAreas, 0, terrain.width * terrain.height * 2);

  std::vector<float> verts;
  verts.reserve(terrain.width * terrain.height * 3);
  for (int y = 0; y < terrain.height; ++y) {
    for (int x = 0; x < terrain.width; ++x) {
      float wx = terrain.minX + static_cast<float>(x) * terrain.cellWidth;
      float wy = terrain.minY + static_cast<float>(y) * terrain.cellHeight;
      float h = terrain.heights[y * terrain.width + x];
      verts.push_back(wx);
      verts.push_back(wy);
      verts.push_back(h);
    }
  }

  std::vector<int> tris;
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

  rcRasterizeTriangles(&ctx, verts.data(), static_cast<int>(verts.size()) / 3,
                       tris.data(), triAreas,
                       static_cast<int>(tris.size()) / 3, *solid, 0);
  delete[] triAreas;

  rcFilterLowHangingWalkableObstacles(&ctx, static_cast<int>(_config.agentMaxClimb / _config.cellHeight), *solid);
  rcFilterLedgeSpans(&ctx, static_cast<int>(_config.agentHeight / _config.cellHeight),
                     static_cast<int>(_config.agentMaxClimb / _config.cellHeight), *solid);
  rcFilterWalkableLowHeightSpans(&ctx, static_cast<int>(_config.agentHeight / _config.cellHeight), *solid);

  rcCompactHeightfield* chf = rcAllocCompactHeightfield();
  if (!chf) {
    rcFreeHeightField(solid);
    return false;
  }
  if (!rcBuildCompactHeightfield(&ctx, static_cast<int>(_config.agentMaxClimb / _config.cellHeight),
                                 static_cast<int>(_config.agentHeight / _config.cellHeight), *solid, *chf)) {
    rcFreeCompactHeightfield(chf);
    rcFreeHeightField(solid);
    return false;
  }
  rcFreeHeightField(solid);

  if (!rcErodeWalkableArea(&ctx, static_cast<int>(_config.agentRadius / _config.cellSize), *chf)) {
    rcFreeCompactHeightfield(chf);
    return false;
  }

  if (!rcBuildRegionsMonotone(&ctx, *chf, 0, _config.minRegionArea, _config.mergeRegionArea)) {
    rcFreeCompactHeightfield(chf);
    return false;
  }

  rcContourSet* cset = rcAllocContourSet();
  if (!cset) {
    rcFreeCompactHeightfield(chf);
    return false;
  }
  if (!rcBuildContours(&ctx, *chf, _config.maxSimplificationError,
                       _config.maxEdgeLen, *cset)) {
    rcFreeContourSet(cset);
    rcFreeCompactHeightfield(chf);
    return false;
  }

  rcPolyMesh* pmesh = rcAllocPolyMesh();
  if (!pmesh) {
    rcFreeContourSet(cset);
    rcFreeCompactHeightfield(chf);
    return false;
  }
  if (!rcBuildPolyMesh(&ctx, *cset, _config.maxVertsPerPoly, *pmesh)) {
    rcFreePolyMesh(pmesh);
    rcFreeContourSet(cset);
    rcFreeCompactHeightfield(chf);
    return false;
  }

  rcPolyMeshDetail* dmesh = rcAllocPolyMeshDetail();
  if (!dmesh) {
    rcFreePolyMesh(pmesh);
    rcFreeContourSet(cset);
    rcFreeCompactHeightfield(chf);
    return false;
  }
  if (!rcBuildPolyMeshDetail(&ctx, *pmesh, *chf, _config.detailSampleDist,
                              _config.detailSampleMaxError, *dmesh)) {
    rcFreePolyMeshDetail(dmesh);
    rcFreePolyMesh(pmesh);
    rcFreeContourSet(cset);
    rcFreeCompactHeightfield(chf);
    return false;
  }

  rcFreeCompactHeightfield(chf);
  rcFreeContourSet(cset);

  for (int i = 0; i < pmesh->npolys; ++i) {
    if (pmesh->areas[i] == RC_WALKABLE_AREA)
      pmesh->flags[i] = 0x01;
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
  params.cs = _config.cellSize;
  params.ch = _config.cellHeight;
  params.buildBvTree = true;

  unsigned char* navData = nullptr;
  int navDataSize = 0;
  if (!dtCreateNavMeshData(&params, &navData, &navDataSize)) {
    rcFreePolyMeshDetail(dmesh);
    rcFreePolyMesh(pmesh);
    return false;
  }

  dtNavMesh* tileNavMesh = dtAllocNavMesh();
  if (!tileNavMesh) {
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

  dtStatus status = tileNavMesh->init(&navParams);
  if (dtStatusFailed(status)) {
    dtFreeNavMesh(tileNavMesh);
    dtFree(navData);
    rcFreePolyMeshDetail(dmesh);
    rcFreePolyMesh(pmesh);
    return false;
  }

  status = tileNavMesh->addTile(navData, navDataSize, DT_TILE_FREE_DATA, 0, nullptr);
  if (dtStatusFailed(status)) {
    dtFreeNavMesh(tileNavMesh);
    dtFree(navData);
    rcFreePolyMeshDetail(dmesh);
    rcFreePolyMesh(pmesh);
    return false;
  }

  SaveNavMeshTile(tileNavMesh, tileX, tileY, outputPath);

  dtFreeNavMesh(tileNavMesh);
  rcFreePolyMeshDetail(dmesh);
  rcFreePolyMesh(pmesh);
  return true;
}

bool MmapGenerator::Generate(uint32_t tileX, uint32_t tileY) {
  TileTerrainData terrain;
  if (!LoadTerrainData(tileX, tileY, terrain))
    return false;

  std::filesystem::create_directories(_config.mmapsDir);
  return BuildTileNavMesh(terrain, tileX, tileY, _config.mmapsDir);
}

bool MmapGenerator::GenerateAllTiles() {
  bool anySuccess = false;
  for (uint32_t tileY = 0; tileY < 64; ++tileY) {
    for (uint32_t tileX = 0; tileX < 64; ++tileX) {
      if (Generate(tileX, tileY))
        anySuccess = true;
    }
  }
  return anySuccess;
}

} // namespace Firelands
