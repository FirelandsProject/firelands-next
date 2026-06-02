#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace firelands::extract {

// Enumerates *.mpq / *.MPQ in `dataDir` and returns paths sorted for Cataclysm-era
// patch application (first entry = SFileOpenArchive, following = SFileOpenPatchArchive).
std::vector<std::filesystem::path>
BuildCataclysmMpqOpenOrder(const std::filesystem::path &dataDir,
                           const std::string &preferredLocale = {});

} // namespace firelands::extract
