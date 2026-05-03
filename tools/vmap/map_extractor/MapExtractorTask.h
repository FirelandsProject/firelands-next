#pragma once

// Top-level map extraction task.
// Opens the MPQ patch chain, reads Map.dbc + the three liquid DBCs, then
// iterates every map and converts each existing ADT tile into a .map file.
// Also extracts cinematic camera M2 models listed in CinematicCamera.dbc into
// outputDir/Cameras/ (reference map_extractor parity).
//
// Callable from firelands-map-extractor / firelands-extractors (TUI launcher).

#include "../common/VMapMagic.h"
#include "AdtReader.h"
#include "MapFileWriter.h"

#include <filesystem>
#include <string>

namespace Firelands::VMap::MapExtractor {

using Firelands::VMap::kTargetBuild;
using Firelands::VMap::kLastDbcInDataBuild;
using Firelands::VMap::kNewBaseSetBuild;

struct MapExtractorOptions {
    std::filesystem::path dataDir;   // WoW Data/ directory
    std::filesystem::path outputDir; // destination root; maps/ sub-dir created
    uint32_t              build{kTargetBuild};
    MapWriteOptions       writeOpts{};
    bool                  verbose{true};
};

// Returns the number of .map files written, or -1 on fatal error.
int RunMapExtractorTask(const MapExtractorOptions& opts);

} // namespace Firelands::VMap::MapExtractor
