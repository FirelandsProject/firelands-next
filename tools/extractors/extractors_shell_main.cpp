#include "ExtractorInteractive.h"

#include <iostream>
#include <string>

int main(int argc, char **argv) {
  if (argc >= 2) {
    const std::string a(argv[1]);
    if (a == "-h" || a == "--help") {
      std::cout
          << "firelands-extractors — interactive menu for DBC/DB2 and map extraction.\n"
             "\n"
             "Run with no arguments to open the console menu.\n"
             "\n"
             "For scripts, use the dedicated tools with flags:\n"
             "  firelands-dbc-extractor  --data <WoW/Data> --out <dir>\n"
             "  firelands-map-extractor  --data <WoW/Data> --out <dir>\n";
      return 0;
    }
    std::cerr << "Unknown argument. Use --help or run with no arguments.\n";
    return 2;
  }
  return firelands::extract::RunInteractiveMenu(argv[0]);
}
