#pragma once

// Command-line parsing for firelands-vmap4-extractor (aligned with map_extractor -d/-o/-b/-q).
// Kept in a small TU so unit tests can validate flags without running MPQ extraction.

#include <cstdint>
#include <filesystem>

namespace Firelands::VMap::Vmap4Extractor {

struct CliOptions {
    std::filesystem::path wowInstall;
    std::filesystem::path outputRoot;
    uint32_t              build{};
    bool                  preciseLargePayload{false};
    bool                  quiet{false};
};

enum class CliParseResult {
    Ok,
    Help, ///< -h / --help / -?
    Error ///< unknown flag, missing value, or missing required -d/-o
};

// Parses argv[1..argc-1]. On Ok, both paths are non-empty.
CliParseResult ParseCli(int argc, char** argv, CliOptions& out);

void PrintUsage(char const* prog);

} // namespace Firelands::VMap::Vmap4Extractor
