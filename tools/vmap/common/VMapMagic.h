#pragma once

#include <cstdint>
#include <cstdio>

// Exact magic strings and version constants used across the vmap pipeline.
// Values must never change — any deviation breaks binary compatibility
// between extractors and the runtime VMapManager.
//
// Reference: reference implementation VMapDefinitions.h

namespace Firelands::VMap {

// Written by vmap4_assembler into final vmaps/; validated by runtime loader.
// 8 bytes, no null terminator in I/O (written with fwrite(..., 8, 1)).
inline constexpr char kVmapMagic[] = "VMAP_4.8";

// Written by vmap4_extractor into intermediate Buildings/ files.
// 8 bytes including the null terminator (fwrite writes the NUL too).
inline constexpr char kRawVmapMagic[] = "VMAP048";

// File name of the game-object model index under vmaps/.
inline constexpr char kGameObjectModels[] = "GameObjectModels.dtree";

// ADT/WDT chunked-file format version validated in MVER chunks.
inline constexpr uint32_t kFileFormatVersion = 18;

// World units per ADT tile (533.33333...).
// Critical: tile indexing arithmetic uses this value.
inline constexpr float kTileSize = 533.33333f;

// Liquid tile size derived from kTileSize.
inline constexpr float kLiquidTileSize = kTileSize / 128.0f;

// Target client build (4.3.4).
inline constexpr uint32_t kTargetBuild = 15595;

// Build gate: DBC archives moved to locale MPQs after this build.
inline constexpr uint32_t kLastDbcInDataBuild = 13623;

// Build gate: new base archive set introduced.
inline constexpr uint32_t kNewBaseSetBuild = 15211;

// Chunk tags written into vmtree files.
inline constexpr char kChunkNode[] = "NODE";
inline constexpr char kChunkSidx[] = "SIDX";

// Chunk tags written into .vmo files.
inline constexpr char kChunkWmod[] = "WMOD";
inline constexpr char kChunkGmod[] = "GMOD";
inline constexpr char kChunkGbih[] = "GBIH";
inline constexpr char kChunkMbih[] = "MBIH";
inline constexpr char kChunkVert[] = "VERT";
inline constexpr char kChunkTrim[] = "TRIM";
inline constexpr char kChunkLiqu[] = "LIQU";

// Chunk tags written into raw Buildings/ group files.
inline constexpr char kChunkGrp[]  = "GRP ";
inline constexpr char kChunkIndx[] = "INDX";

// Validation helper: reads exactly `len` bytes, compares to `expected`.
// Returns true if they match. Used when opening vmtree/vmtile/vmo.
bool ReadAndValidateChunk(FILE* rf, const char* expected, uint32_t len);

} // namespace Firelands::VMap
