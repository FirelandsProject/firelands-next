#include "Vmap4ExtractorCli.h"

#include "../common/VMapMagic.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace Firelands::VMap::Vmap4Extractor {

void PrintUsage(char const* prog) {
    std::printf(
        "Firelands VMAP4 extractor (WoW 4.3.x — buildings + ADT dir binaries)\n"
        "Usage:\n"
        "  %s -d <WoW-dir> -o <output-dir> [-b <build>] [-s|-l] [-q]\n"
        "\n"
        "Options:\n"
        "  -d  Path to the WoW install directory (must contain Data/). Required.\n"
        "  -o  Output root directory. Writes <output-dir>/Buildings/ and dir files. Required.\n"
        "  -b  Target client build (default %u).\n"
        "  -s  Smaller collision payloads (default).\n"
        "  -l  Larger payloads (more detail; roughly +500 MB).\n"
        "  -q  Quiet — suppress MPQ loading and per-map progress output.\n"
        "  -h  --help  Show this message.\n",
        prog, static_cast<unsigned>(kTargetBuild));
}

CliParseResult ParseCli(int argc, char** argv, CliOptions& out) {
    out                     = {};
    out.build               = kTargetBuild;
    out.preciseLargePayload = false;
    out.quiet               = false;

    bool has_d = false;
    bool has_o = false;

    for (int i = 1; i < argc; ++i) {
        std::string const arg(argv[i]);
        if (arg == "-h" || arg == "--help" || arg == "-?") {
            return CliParseResult::Help;
        }
        if (arg == "-d" && i + 1 < argc) {
            out.wowInstall = argv[++i];
            has_d          = true;
        } else if (arg == "-o" && i + 1 < argc) {
            out.outputRoot = argv[++i];
            has_o          = true;
        } else if (arg == "-b") {
            if (i + 1 >= argc) {
                return CliParseResult::Error;
            }
            out.build = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "-s") {
            out.preciseLargePayload = false;
        } else if (arg == "-l") {
            out.preciseLargePayload = true;
        } else if (arg == "-q") {
            out.quiet = true;
        } else {
            return CliParseResult::Error;
        }
    }

    if (!has_d || !has_o) {
        return CliParseResult::Error;
    }
    return CliParseResult::Ok;
}

} // namespace Firelands::VMap::Vmap4Extractor
