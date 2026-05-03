#pragma once

// .map file writer.
// Takes the fully populated AdtGridData from AdtReader and serialises it to
// the binary .map format consumed by the server's MapManager.
//
// Output format matches reference implementation System.cpp
// ConvertADT() exactly:
//
//   map_fileheader  (44 bytes)
//   map_areaHeader  (8 bytes)     [+ optional uint16 area_ids[16][16]]
//   map_heightHeader (16 bytes)   [+ optional height data]
//   [optional flight box int16[3][3] × 2]
//   [optional map_liquidHeader + liquid data]
//   [optional uint16 holes[16][16]]
//
// The .tilelist sidecar file (one per map ID) is also written here.

#include "MapTypes.h"
#include "AdtReader.h"

#include <bitset>
#include <cstdint>
#include <filesystem>
#include <string>

namespace Firelands::VMap::MapExtractor {

struct MapWriteOptions {
    bool  allowHeightLimit  = true;
    float minHeight         = kDefaultMinHeight;
    bool  allowFloatToInt   = true;
    float floatToInt8Limit  = kFloatToInt8Limit;
    float floatToInt16Limit = kFloatToInt16Limit;
    float flatHeightDelta   = kFlatHeightDelta;
    float flatLiquidDelta   = kFlatLiquidDelta;
};

class MapFileWriter {
public:
    // Write a single .map tile file.
    // `grid`    — produced by AdtReader::Parse().
    // `outPath` — full path, e.g. "maps/001 00 32.map" (no spaces, shown for clarity).
    //             Actual pattern: "%03u%02u%02u.map" (mapId, tileY, tileX).
    // `build`   — client build number written into the header.
    // `opts`    — packing options (defaults match reference).
    // Returns false on I/O error.
    static bool Write(const AdtGridData& grid,
                      const std::filesystem::path& outPath,
                      uint32_t build,
                      const MapWriteOptions& opts = {});

    // Write a .tilelist sidecar file for one map ID.
    // `outPath`      — full path, e.g. "maps/001.tilelist".
    // `existingTiles`— bitset[64×64] from WdtReader (true = tile was converted).
    // `build`        — client build.
    static bool WriteTileList(const std::filesystem::path& outPath,
                              const std::bitset<kWdtMapSize * kWdtMapSize>& existingTiles,
                              uint32_t build);
};

} // namespace Firelands::VMap::MapExtractor
