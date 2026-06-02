#include "MmapGenerator.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>

void PrintUsage() {
  printf("firelands-mmap-generator - Build navmesh tiles from server .map data\n");
  printf("Usage: firelands-mmap-generator -m <mapId|all> -i <mapsDir> -o <mmapsDir> [-v <vmapsDir>]\n");
  printf("\n");
  printf("  -m <mapId>   Map ID to generate navmesh for (e.g. 0 for Eastern Kingdoms)\n");
  printf("  -m all       Generate navmesh for every mapId found in <mapsDir>\n");
  printf("  --all-maps   Same as -m all\n");
  printf("  -i <dir>     Input directory containing server .map files\n");
  printf("  -o <dir>     Output directory for .mmtile files\n");
  printf("  -v <dir>     Optional: vmaps directory for building collision\n");
  printf("  -t <x> <y>   Optional: generate only tile (x,y) instead of all\n");
  printf("  -h           Show this help\n");
}

std::set<uint32_t> DiscoverMapIds(std::string const& mapsDir) {
  std::set<uint32_t> mapIds;
  if (!std::filesystem::is_directory(mapsDir))
    return mapIds;

  for (auto const& entry : std::filesystem::directory_iterator(mapsDir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".map")
      continue;

    std::string const stem = entry.path().stem().string();
    if (stem.size() != 7)
      continue;

    bool numeric = true;
    for (char c : stem) {
      if (c < '0' || c > '9') {
        numeric = false;
        break;
      }
    }
    if (!numeric)
      continue;

    mapIds.insert(static_cast<uint32_t>(std::stoul(stem.substr(0, 3))));
  }
  return mapIds;
}

uint32_t CountExistingTiles(std::string const& mapsDir, uint32_t mapId) {
  uint32_t totalTiles = 0;
  char stemPrefix[4];
  std::snprintf(stemPrefix, sizeof(stemPrefix), "%03u", mapId);

  if (!std::filesystem::is_directory(mapsDir))
    return 0;

  for (auto const& entry : std::filesystem::directory_iterator(mapsDir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".map")
      continue;

    std::string const stem = entry.path().stem().string();
    if (stem.size() == 7 && stem.rfind(stemPrefix, 0) == 0)
      ++totalTiles;
  }

  return totalTiles;
}

int main(int argc, char* argv[]) {
  Firelands::MmapGeneratorConfig config = Firelands::MmapGeneratorConfig::Default();
  bool hasMapId = false;
  bool allMaps = false;
  bool hasInput = false;
  bool hasOutput = false;
  bool singleTile = false;
  uint32_t tileX = 0;
  uint32_t tileY = 0;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "-h") == 0) {
      PrintUsage();
      return 0;
    }
    if (std::strcmp(argv[i], "--all-maps") == 0) {
      allMaps = true;
      hasMapId = true;
    } else if (std::strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
      char const* value = argv[++i];
      if (std::strcmp(value, "all") == 0 || std::strcmp(value, "ALL") == 0) {
        allMaps = true;
      } else {
        config.mapId = static_cast<uint32_t>(std::atoi(value));
      }
      hasMapId = true;
    } else if (std::strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
      config.mapsDir = argv[++i];
      hasInput = true;
    } else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      config.mmapsDir = argv[++i];
      hasOutput = true;
    } else if (std::strcmp(argv[i], "-v") == 0 && i + 1 < argc) {
      config.vmapsDir = argv[++i];
    } else if (std::strcmp(argv[i], "-t") == 0 && i + 2 < argc) {
      tileX = static_cast<uint32_t>(std::atoi(argv[++i]));
      tileY = static_cast<uint32_t>(std::atoi(argv[++i]));
      singleTile = true;
    }
  }

  if (!hasMapId || !hasInput || !hasOutput) {
    PrintUsage();
    return 1;
  }

  if (!std::filesystem::exists(config.mapsDir)) {
    fprintf(stderr, "Error: maps directory not found: %s\n", config.mapsDir.c_str());
    return 1;
  }

  if (allMaps && singleTile) {
    fprintf(stderr, "Error: -t can only be used with one mapId, not -m all.\n");
    return 1;
  }

  if (allMaps) {
    auto const mapIds = DiscoverMapIds(config.mapsDir);
    if (mapIds.empty()) {
      fprintf(stderr, "Error: no *.map files found in: %s\n", config.mapsDir.c_str());
      return 1;
    }

    uint32_t totalTiles = 0;
    for (uint32_t mapId : mapIds)
      totalTiles += CountExistingTiles(config.mapsDir, mapId);

    printf("\nFirelands mmap generator\n");
    printf("========================\n");
    printf("Maps: %zu detected\n", mapIds.size());
    printf("Tiles: %u existing terrain tiles total\n\n", totalTiles);

    uint32_t generatedMaps = 0;
    uint32_t failedMaps = 0;
    uint32_t processedMaps = 0;
    uint32_t processedTiles = 0;
    for (uint32_t mapId : mapIds) {
      ++processedMaps;
      uint32_t const mapTiles = CountExistingTiles(config.mapsDir, mapId);
      Firelands::MmapGeneratorConfig mapConfig = config;
      mapConfig.mapId = mapId;
      Firelands::MmapGenerator generator(std::move(mapConfig));

      printf("\n============================================================\n");
      printf("Generating map %u/%zu: %u (%u tile(s), %u/%u global done)\n",
             processedMaps, mapIds.size(), mapId, mapTiles, processedTiles,
             totalTiles);
      printf("============================================================\n");
      Firelands::MmapGenerator::BatchProgress progress;
      progress.mapIndex = processedMaps;
      progress.mapCount = static_cast<uint32_t>(mapIds.size());
      progress.globalProcessedTiles = processedTiles;
      progress.globalTotalTiles = totalTiles;

      if (generator.GenerateAllTiles(&progress)) {
        ++generatedMaps;
      } else {
        ++failedMaps;
      }
      processedTiles += mapTiles;
    }

    printf("\nAll-map generation complete: %u/%zu map(s) generated, %u map(s) failed, %u/%u tile(s) processed.\n",
           generatedMaps, mapIds.size(), failedMaps, processedTiles,
           totalTiles);
    return failedMaps == 0 ? 0 : 1;
  }

  uint32_t const mapId = config.mapId;
  Firelands::MmapGenerator generator(std::move(config));

  printf("\nFirelands mmap generator\n");
  printf("========================\n");
  printf("Map: %u\n", mapId);

  if (singleTile) {
    printf("Tile: (%u,%u)\n\n", tileX, tileY);
    printf("Generating navmesh...\n");
    if (!generator.Generate(tileX, tileY)) {
      fprintf(stderr, "Failed to generate tile (%u,%u).\n", tileX, tileY);
      return 1;
    }
    printf("\nTile (%u,%u) generated successfully.\n", tileX, tileY);
  } else {
    printf("Tiles: all 64x64\n\n");
    printf("Generating navmesh...\n");
    if (!generator.GenerateAllTiles()) {
      fprintf(stderr, "No tiles were generated. Check that .map files exist "
                      "in the input directory.\n");
      return 1;
    }
    printf("\nNavmesh generation complete.\n");
  }

  return 0;
}
