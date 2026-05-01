#pragma once

#include <filesystem>
#include <iosfwd>

namespace firelands::extract {

// Returns 0 on success, non-zero on failure (see stderr-style messages via err).
int RunListMpqsTask(const std::filesystem::path &dataDir, std::ostream &out,
                    std::ostream &err);

int RunDbcExtractTask(const std::filesystem::path &dataDir,
                      const std::filesystem::path &outDir, std::ostream &out,
                      std::ostream &err);

int RunMapExtractTask(const std::filesystem::path &dataDir,
                      const std::filesystem::path &outDir, std::ostream &out,
                      std::ostream &err);

} // namespace firelands::extract
