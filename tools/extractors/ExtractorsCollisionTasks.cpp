// Collision-pipeline tasks for firelands-extractors TUI only (not linked into
// FirelandsExtractCommon — needs FirelandsMapExtractorLib + subprocess paths).

#include "ExtractorTasks.h"

#include "MapExtractorTask.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace firelands::extract {
namespace {

static std::string ShellQuoteArg(std::string const& s) {
    if (s.find_first_of(" \t\n\r\"'$`") == std::string::npos) {
        return s;
    }
    std::string r = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') {
            r.push_back('\\');
        }
        r.push_back(c);
    }
    r.push_back('"');
    return r;
}

static int InterpretSystemReturn(int st) {
#ifndef _WIN32
    if (WIFEXITED(st)) {
        return WEXITSTATUS(st);
    }
    return -1;
#else
    return st;
#endif
}

static int RunProcessCapture(std::string const& command, std::ostream& out, std::ostream& err) {
    out << "[spawn] " << command << "\n";
    int const st   = std::system(command.c_str());
    int const code = InterpretSystemReturn(st);
    if (code != 0) {
        err << "[error] subprocess exited with code " << code << "\n";
    }
    return code;
}

} // namespace

int RunServerMapVmapExtractTask(const std::filesystem::path& dataDir,
                                 const std::filesystem::path& outDir, std::ostream& out,
                                 std::ostream& err) {
    if (!std::filesystem::is_directory(dataDir)) {
        err << "Data directory missing or not a directory: " << dataDir.string() << "\n";
        return 1;
    }
    Firelands::VMap::MapExtractor::MapExtractorOptions opts;
    opts.dataDir   = dataDir;
    opts.outputDir = outDir;
    opts.verbose   = true;
    int const n    = Firelands::VMap::MapExtractor::RunMapExtractorTask(opts);
    if (n < 0) {
        err << "Server map extraction failed (MPQ or DBC error).\n";
        return 1;
    }
    out << "Wrote " << n << " .map tile(s) under " << (outDir / "maps").string() << "\n";
    return 0;
}

int RunVmap4ExtractorSubprocess(const std::filesystem::path& wowDataDir,
                                const std::filesystem::path& collisionOutRoot, std::ostream& out,
                                std::ostream& err) {
    if (!std::filesystem::is_directory(wowDataDir)) {
        err << "Data directory missing: " << wowDataDir.string() << "\n";
        return 1;
    }
    std::filesystem::path const wowInstall = wowDataDir.parent_path();
#ifndef FIRELANDS_VMAP4_EXTRACTOR_PATH
#error "FIRELANDS_VMAP4_EXTRACTOR_PATH must be set by CMake for firelands-extractors"
#endif
    std::ostringstream cmd;
    cmd << ShellQuoteArg(FIRELANDS_VMAP4_EXTRACTOR_PATH) << " -d "
        << ShellQuoteArg(wowInstall.string()) << " -o " << ShellQuoteArg(collisionOutRoot.string())
        << " -q";
    return RunProcessCapture(cmd.str(), out, err) == 0 ? 0 : 1;
}

int RunVmap4AssemblerSubprocess(const std::filesystem::path& collisionOutRoot, std::ostream& out,
                                std::ostream& err) {
    std::filesystem::path const buildings = collisionOutRoot / "Buildings";
    std::filesystem::path const vmaps     = collisionOutRoot / "vmaps";
    if (!std::filesystem::is_directory(buildings)) {
        err << "Buildings/ not found under " << collisionOutRoot.string()
            << " — run VMAP4 extract first.\n";
        return 1;
    }
#ifndef FIRELANDS_VMAP4_ASSEMBLER_PATH
#error "FIRELANDS_VMAP4_ASSEMBLER_PATH must be set by CMake for firelands-extractors"
#endif
    std::ostringstream cmd;
    cmd << ShellQuoteArg(FIRELANDS_VMAP4_ASSEMBLER_PATH) << " "
        << ShellQuoteArg(buildings.string()) << " " << ShellQuoteArg(vmaps.string());
    return RunProcessCapture(cmd.str(), out, err) == 0 ? 0 : 1;
}

} // namespace firelands::extract
