#include "TileAssembler.h"

#include <cstdio>
#include <iostream>
#include <string>

static void PrintUsage(char const* prog) {
    std::fprintf(stderr,
                   "Firelands vmap4 assembler (Buildings/ -> vmaps/)\n"
                   "Usage:\n"
                   "  %s [source_dir] [dest_dir]\n"
                   "\n"
                   "  source_dir  Directory containing dir_bin and raw model files (default: Buildings)\n"
                   "  dest_dir    Output directory for .vmtree, .vmtile, .vmo (default: vmaps)\n",
                   prog);
}

int main(int argc, char** argv) {
    if (argc > 1) {
        std::string a(argv[1]);
        if (a == "-h" || a == "--help") {
            PrintUsage(argv[0]);
            return 0;
        }
    }

    std::string src = "Buildings";
    std::string dest = "vmaps";

    if (argc > 3) {
        PrintUsage(argv[0]);
        return 1;
    }
    if (argc > 1) {
        src = argv[1];
    }
    if (argc > 2) {
        dest = argv[2];
    }

    std::cout << "Using \"" << src << "\" as source directory and writing output to \"" << dest
              << "\"\n";

    Firelands::VMap::Assembler::TileAssembler ta(src, dest);
    if (!ta.convertWorld2()) {
        std::cout << "exit with errors\n";
        return 1;
    }

    std::cout << "Ok, all done\n";
    return 0;
}
