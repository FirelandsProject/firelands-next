#pragma once

// ADT file parser for the map extractor.
// Reads an ADT tile from an MPQ stream and populates all grid arrays needed
// to write a .map file:
//   V9[129][129]  — outer vertex heights  (corner grid)
//   V8[128][128]  — inner vertex heights  (diamond grid)
//   area_ids[16][16]
//   liquid_entry[16][16], liquid_flags[16][16]
//   liquid_show[128][128], liquid_height[129][129]
//   holes[16][16]
//   flight_box_max/min[3][3]  (only when MFBO present)
//
// Ported from ConvertADT() in
//   reference implementation System.cpp

#include "MapTypes.h"
#include "../common/ChunkReader.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace Firelands::VMap::MapExtractor {

// DBC tables needed for MH2O liquid type resolution (filled by MapExtractorTask).
struct LiquidDbcTables {
    std::unordered_map<uint32_t, LiquidMaterialEntry> materials;
    std::unordered_map<uint32_t, LiquidObjectEntry>   objects;
    std::unordered_map<uint32_t, LiquidTypeEntry>     types;
};

// All per-tile intermediate data produced by AdtReader::Parse().
struct AdtGridData {
    float    V9[kAdtGridSize + 1][kAdtGridSize + 1]{};  // outer vertices (129×129)
    float    V8[kAdtGridSize][kAdtGridSize]{};           // inner vertices (128×128)

    uint16_t area_ids[kAdtCellsPerGrid][kAdtCellsPerGrid]{};

    uint16_t liquid_entry[kAdtCellsPerGrid][kAdtCellsPerGrid]{};
    uint8_t  liquid_flags[kAdtCellsPerGrid][kAdtCellsPerGrid]{};
    bool     liquid_show[kAdtGridSize][kAdtGridSize]{};
    float    liquid_height[kAdtGridSize + 1][kAdtGridSize + 1]{};

    uint16_t holes[kAdtCellsPerGrid][kAdtCellsPerGrid]{};

    bool     hasHoles{};
    bool     hasFlightBox{};
    int16_t  flight_box_max[3][3]{};
    int16_t  flight_box_min[3][3]{};
};

class AdtReader {
public:
    // Parse an ADT file from an in-memory buffer.
    // `buf` / `size` come from MpqStream::Data() / Size().
    // `dbc`   — resolved DBC lookup tables (must be filled before calling).
    // `ignoreDeepWater` — suppress dark water flag for specific grids.
    // Returns false if the buffer is not a valid MVER=18 ADT.
    static bool Parse(const uint8_t* buf, uint32_t size,
                      const LiquidDbcTables& dbc,
                      bool ignoreDeepWater,
                      AdtGridData& out);

private:
    // Resolve LiquidVertexFormat for an MH2O liquid instance.
    static LiquidVertexFormat GetLiquidVertexFormat(
        const uint8_t* mh2oBase,
        const adt_liquid_instance* h,
        const LiquidDbcTables& dbc);

    // Height at position `pos` in an MH2O vertex data block.
    static float GetMh2oLiquidHeight(const uint8_t* mh2oBase,
                                     const adt_liquid_instance* h,
                                     LiquidVertexFormat fmt, int pos);

    // Process one MCNK chunk (fills V8/V9, area_ids, legacy liquid, holes).
    static void ProcessMcnk(const uint8_t* mcnkData,
                             bool ignoreDeepWater,
                             AdtGridData& out);

    // Process MH2O chunk (Wrath/Cata liquid system).
    static void ProcessMh2o(const uint8_t* mh2oData, uint32_t mh2oPayloadSize,
                             const LiquidDbcTables& dbc,
                             bool ignoreDeepWater,
                             AdtGridData& out);
};

} // namespace Firelands::VMap::MapExtractor
