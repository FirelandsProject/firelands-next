#ifndef FIRELANDS_MMAP_GENERATOR_MMAP_GENERATOR_H
#define FIRELANDS_MMAP_GENERATOR_MMAP_GENERATOR_H

#include <cstdint>
#include <string>
#include <vector>

namespace Firelands {

struct MmapGeneratorConfig {
  std::string mapsDir;
  std::string vmapsDir;
  std::string mmapsDir;
  uint32_t mapId = 0;

  float cellSize = 0.3f;
  float cellHeight = 0.2f;
  float agentHeight = 2.0f;
  float agentRadius = 0.6f;
  float agentMaxClimb = 0.9f;
  float agentMaxSlope = 45.0f;
  float minRegionArea = 10.0f;
  float mergeRegionArea = 20.0f;
  float maxEdgeLen = 12.0f;
  float maxSimplificationError = 1.3f;
  int maxVertsPerPoly = 6;
  float detailSampleDist = 6.0f;
  float detailSampleMaxError = 1.0f;

  static MmapGeneratorConfig Default() { return {}; }
};

class MmapGenerator {
public:
  explicit MmapGenerator(MmapGeneratorConfig config);

  bool Generate(uint32_t tileX, uint32_t tileY);
  bool GenerateAllTiles();

private:
  struct TileTerrainData {
    std::vector<float> heights;
    int width = 0;
    int height = 0;
    float minX = 0.0f;
    float minY = 0.0f;
    float minZ = 0.0f;
    float maxZ = 0.0f;
    float cellSize = 0.0f;
  };

  bool LoadTerrainData(uint32_t tileX, uint32_t tileY, TileTerrainData& out) const;
  bool BuildTileNavMesh(TileTerrainData const& terrain, uint32_t tileX,
                        uint32_t tileY, std::string const& outputPath) const;

  MmapGeneratorConfig _config;
};

} // namespace Firelands

#endif
