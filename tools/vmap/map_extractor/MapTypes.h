#pragma once

// On-disk and in-memory layouts for WoW 4.3.4 map files consumed by the
// map extractor.  Ported byte-for-byte from:
//   reference implementation adt.h
//   reference implementation wdt.h
//   reference implementation loadlib.h (System.cpp constants)
//
// All structs are #pragma pack(1) to match the binary format exactly.

#include <cstdint>

namespace Firelands::VMap::MapExtractor {

// ─── Grid constants ───────────────────────────────────────────────────────────

inline constexpr float  kTileSize   = 533.33333f;
inline constexpr float  kChunkSize  = kTileSize / 16.0f;
inline constexpr float  kUnitSize   = kChunkSize / 8.0f;

inline constexpr int    kAdtCellsPerGrid = 16;
inline constexpr int    kAdtCellSize     = 8;
inline constexpr int    kAdtGridSize     = kAdtCellsPerGrid * kAdtCellSize; // 128

inline constexpr int    kWdtMapSize = 64; // WDT grid is 64×64

// ─── .map output format constants ────────────────────────────────────────────

// Header magic: "MAPS" stored as little-endian uint32
inline constexpr uint32_t kMapMagic        = 0x5350414Du; // 'MAPS'
inline constexpr uint32_t kMapVersionMagic = 10;

inline constexpr uint32_t kMapAreaMagic   = 0x41455241u; // 'AREA'
inline constexpr uint32_t kMapHeightMagic = 0x54474D48u; // 'MHGT'
inline constexpr uint32_t kMapLiquidMagic = 0x51494C4Du; // 'MLIQ'

// Area header flags
inline constexpr uint16_t kMapAreaNoArea = 0x0001;

// Height header flags
inline constexpr uint32_t kMapHeightNoHeight       = 0x0001;
inline constexpr uint32_t kMapHeightAsInt16        = 0x0002;
inline constexpr uint32_t kMapHeightAsInt8         = 0x0004;
inline constexpr uint32_t kMapHeightHasFlightBounds = 0x0008;

// Liquid header flags
inline constexpr uint8_t  kMapLiquidNoType   = 0x01;
inline constexpr uint8_t  kMapLiquidNoHeight = 0x02;

// Liquid type bits stored in liquid_flags array
inline constexpr uint8_t  kLiquidFlagWater     = 0x01;
inline constexpr uint8_t  kLiquidFlagOcean     = 0x02;
inline constexpr uint8_t  kLiquidFlagMagma     = 0x04;
inline constexpr uint8_t  kLiquidFlagSlime     = 0x08;
inline constexpr uint8_t  kLiquidFlagDarkWater = 0x10;

// MCLQ rendering-hidden flag
inline constexpr uint8_t  kMclqHiddenFlag = 0x0F;

// ADT liquid header flags
inline constexpr uint8_t  kAdtLiquidHeaderFullLight = 0x01;
inline constexpr uint8_t  kAdtLiquidHeaderNoHeight  = 0x02;

// Height clamping defaults
inline constexpr float    kDefaultMinHeight   = -2000.0f;
inline constexpr float    kFlatHeightDelta    = 0.005f;
inline constexpr float    kFlatLiquidDelta    = 0.001f;
inline constexpr float    kFloatToInt8Limit   = 2.0f;
inline constexpr float    kFloatToInt16Limit  = 2048.0f;

// ─── LiquidType sound-bank IDs ────────────────────────────────────────────────

enum LiquidSoundBank : uint8_t {
    kLiquidSoundWater = 0,
    kLiquidSoundOcean = 1,
    kLiquidSoundMagma = 2,
    kLiquidSoundSlime = 3,
};

// ─── Liquid vertex format (MH2O) ──────────────────────────────────────────────

enum class LiquidVertexFormat : uint16_t {
    HeightDepth             = 0,
    HeightTextureCoord      = 1,
    Depth                   = 2,
    HeightDepthTextureCoord = 3,
    Unk4                    = 4,
    Unk5                    = 5,
    Invalid                 = 0xFFFFu,
};

// ─────────────────────────── ADT on-disk structs ─────────────────────────────

#pragma pack(push, 1)

// MCVT sub-chunk: heightmap vertex data
struct adt_MCVT {
    uint32_t fcc;
    uint32_t size;
    // (CELL_SIZE+1)*(CELL_SIZE+1) outer vertices + CELL_SIZE*CELL_SIZE inner ones
    float height_map[(kAdtCellSize + 1) * (kAdtCellSize + 1) + kAdtCellSize * kAdtCellSize];
};

// MCLQ sub-chunk: old-style liquid
struct adt_MCLQ {
    uint32_t fcc;
    uint32_t size;
    float height1;
    float height2;
    struct liquid_vertex {
        uint32_t light;
        float    height;
    } liquid[kAdtCellSize + 1][kAdtCellSize + 1];
    uint8_t  flags[kAdtCellSize][kAdtCellSize];
    uint8_t  data[84];
};

// MCNK chunk: one 16th×16th terrain cell
struct adt_MCNK {
    uint32_t fcc;
    uint32_t size;
    uint32_t flags;
    uint32_t ix;
    uint32_t iy;
    uint32_t nLayers;
    uint32_t nDoodadRefs;
    uint32_t offsMCVT;
    uint32_t offsMCNR;
    uint32_t offsMCLY;
    uint32_t offsMCRF;
    uint32_t offsMCAL;
    uint32_t sizeMCAL;
    uint32_t offsMCSH;
    uint32_t sizeMCSH;
    uint32_t areaid;
    uint32_t nMapObjRefs;
    uint32_t holes;
    uint16_t s[2];
    uint32_t data1;
    uint32_t data2;
    uint32_t data3;
    uint32_t predTex;
    uint32_t nEffectDoodad;
    uint32_t offsMCSE;
    uint32_t nSndEmitters;
    uint32_t offsMCLQ;
    uint32_t sizeMCLQ;
    float    zpos;   // position in WoW coords
    float    xpos;
    float    ypos;
    uint32_t offsMCCV;
    uint32_t props;
    uint32_t effectId;
};

// MH2O per-cell liquid instance
struct adt_liquid_instance {
    uint16_t LiquidType;
    uint16_t LiquidVertexFormat;   // if >= 42 → look up in LiquidObject.dbc
    float    MinHeightLevel;
    float    MaxHeightLevel;
    uint8_t  OffsetX;
    uint8_t  OffsetY;
    uint8_t  Width;
    uint8_t  Height;
    uint32_t OffsetExistsBitmap;
    uint32_t OffsetVertexData;

    uint8_t GetEffectiveOffsetX() const { return LiquidVertexFormat < 42 ? OffsetX : 0; }
    uint8_t GetEffectiveOffsetY() const { return LiquidVertexFormat < 42 ? OffsetY : 0; }
    uint8_t GetEffectiveWidth()   const { return LiquidVertexFormat < 42 ? Width  : 8; }
    uint8_t GetEffectiveHeight()  const { return LiquidVertexFormat < 42 ? Height : 8; }
};

struct adt_liquid_attributes {
    uint64_t Fishable;
    uint64_t Deep;
};

// MH2O per-cell header (16×16 grid)
struct adt_LIQUID_CELL {
    uint32_t OffsetInstances;
    uint32_t used;
    uint32_t OffsetAttributes;
};

// MFBO chunk: flight bounds plane data
struct adt_MFBO {
    uint32_t fcc;
    uint32_t size;
    struct plane { int16_t coords[9]; };
    plane max;
    plane min;
};

// WDT MAIN chunk: 64×64 tile existence flags
struct wdt_MAIN {
    uint32_t fcc;
    uint32_t size;
    struct adtData {
        uint32_t flag;
        uint32_t data1;
    } adt_list[kWdtMapSize][kWdtMapSize];
};

#pragma pack(pop)

// ─── .map output binary structs ──────────────────────────────────────────────

#pragma pack(push, 1)

struct map_fileheader {
    uint32_t mapMagic;
    uint32_t versionMagic;
    uint32_t buildMagic;
    uint32_t areaMapOffset;
    uint32_t areaMapSize;
    uint32_t heightMapOffset;
    uint32_t heightMapSize;
    uint32_t liquidMapOffset;
    uint32_t liquidMapSize;
    uint32_t holesOffset;
    uint32_t holesSize;
};

struct map_areaHeader {
    uint32_t fourcc;
    uint16_t flags;
    uint16_t gridArea;
};

struct map_heightHeader {
    uint32_t fourcc;
    uint32_t flags;
    float    gridHeight;
    float    gridMaxHeight;
};

struct map_liquidHeader {
    uint32_t fourcc;
    uint8_t  flags;
    uint8_t  liquidFlags;
    uint16_t liquidType;
    uint8_t  offsetX;
    uint8_t  offsetY;
    uint8_t  width;
    uint8_t  height;
    float    liquidLevel;
};

#pragma pack(pop)

// ─── DBC lookup tables ───────────────────────────────────────────────────────

struct LiquidMaterialEntry { int8_t  LVF;         };
struct LiquidObjectEntry   { int16_t LiquidTypeID; };
struct LiquidTypeEntry     { uint8_t SoundBank; uint8_t MaterialID; };

} // namespace Firelands::VMap::MapExtractor
