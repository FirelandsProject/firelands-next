#include "MmapGenerator.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

void PrintUsage() {
  printf("firelands-mmap-generator - Build navmesh tiles from server .map data\n");
  printf("Usage: firelands-mmap-generator -m <mapId> -i <mapsDir> -o <mmapsDir> [-v <vmapsDir>]\n");
  printf("\n");
  printf("  -m <mapId>   Map ID to generate navmesh for (e.g. 0 for Eastern Kingdoms)\n");
  printf("  -i <dir>     Input directory containing server .map files\n");
  printf("  -o <dir>     Output directory for .mmtile files\n");
  printf("  -v <dir>     Optional: vmaps directory for building collision\n");
  printf("  -t <x> <y>   Optional: generate only tile (x,y) instead of all\n");
  printf("  -h           Show this help\n");
}

int main(int argc, char* argv[]) {
  Firelands::MmapGeneratorConfig config = Firelands::MmapGeneratorConfig::Default();
  bool hasMapId = false;
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
    if (std::strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
      config.mapId = static_cast<uint32_t>(std::atoi(argv[++i]));
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
