#include "ExtractorsFtxui.h"

#include <iostream>
#include <string>

int main(int argc, char **argv) {
  if (argc >= 2) {
    const std::string a(argv[1]);
    if (a == "-h" || a == "--help") {
      std::cout
          << "firelands-extractors — fullscreen TUI to configure and run client-data tasks.\n"
             "\n"
             "Run with no arguments to open the interactive launcher (banner + console).\n"
             "\n"
             "Non-interactive / CI (no TTY):\n"
             "  firelands-dbc-extractor          --data <WoW/Data> --out <dir> [--list-mpqs]\n"
             "  firelands-map-extractor          --data <WoW/Data> --out <dir> [--list-mpqs]\n"
             "  firelands-map-extractor-vmap     -d <WoW-dir> -o <dir>   (WoW dir contains Data/)\n"
             "  firelands-vmap4-extractor        -d <WoW-dir> -o <collision-root>\n"
             "  firelands-vmap4-assembler        [Buildings-dir] [vmaps-dir]\n";
      return 0;
    }
    std::cerr << "Unknown argument. Use --help.\n";
    return 2;
  }
  return firelands::extract::RunExtractorsFtxui();
}
