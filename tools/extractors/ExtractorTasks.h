#pragma once

#include <filesystem>
#include <iosfwd>
#include <string>

namespace firelands::extract {

// Returns 0 on success, non-zero on failure (see stderr-style messages via err).
int RunListMpqsTask(const std::filesystem::path &dataDir, std::ostream &out,
                    std::ostream &err, const std::string &locale = {});

int RunDbcExtractTask(const std::filesystem::path &dataDir,
                      const std::filesystem::path &outDir, std::ostream &out,
                      std::ostream &err, const std::string &locale = {});

int RunMapExtractTask(const std::filesystem::path &dataDir,
                      const std::filesystem::path &outDir, std::ostream &out,
                      std::ostream &err, const std::string &locale = {});

// Server collision maps: `maps/*.map`, tilelists, `Cameras/` (MPQ via map_extractor task).
int RunServerMapVmapExtractTask(const std::filesystem::path &dataDir,
                                const std::filesystem::path &outDir, std::ostream &out,
                                std::ostream &err);

// VMAP4 extractor: ADT/WMO/M2 → `Buildings/` (runs `firelands-vmap4-extractor` subprocess).
int RunVmap4ExtractorSubprocess(const std::filesystem::path &wowDataDir,
                                const std::filesystem::path &collisionOutRoot,
                                std::ostream &out, std::ostream &err);

// VMAP4 assembler: `Buildings/` → `vmaps/` (runs `firelands-vmap4-assembler` subprocess).
int RunVmap4AssemblerSubprocess(const std::filesystem::path &collisionOutRoot,
                                std::ostream &out, std::ostream &err);

} // namespace firelands::extract
