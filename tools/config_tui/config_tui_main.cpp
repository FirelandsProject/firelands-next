#include "ConfigEditorFtxui.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void PrintUsage(char const *exe) {
  std::cerr << "Usage: " << (exe ? exe : "firelands-config") << " [options]\n"
            << "  Interactive TTY editor for authserver.yaml and worldserver.yaml.\n"
            << "Options:\n"
            << "  --auth <path>   Override auth config file path\n"
            << "  --world <path>  Override world config file path\n"
            << "  -h, --help      Show this help\n";
}

} // namespace

int main(int argc, char **argv) {
  for (int i = 1; i < argc; ++i) {
    std::string const a = argv[i];
    if (a == "-h" || a == "--help") {
      PrintUsage(argv[0]);
      return 0;
    }
  }
  return RunConfigEditorFtxui(argc, argv);
}
