#include "ExtractorInteractive.h"
#include "ExtractorTasks.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

void PrintUsage(const char *prog) {
  std::cerr << "Usage:\n"
            << "  " << prog << "                     (interactive menu)\n"
            << "  " << prog << " --data <WoW/Data> --out <output_dir> [options]\n"
            << "Extracts DBFilesClient\\*.dbc and DBFilesClient\\*.db2 from the MPQ chain.\n"
            << "Options:\n"
            << "  --list-mpqs   Print resolved MPQ open order and exit\n";
}

bool ArgMatch(int argc, char **argv, int i, const char *flag) {
  return i < argc && std::string(argv[i]) == flag;
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 1) {
    return firelands::extract::RunInteractiveMenu(argv[0]);
  }

  std::filesystem::path dataDir;
  std::filesystem::path outDir;
  bool listOnly = false;

  for (int i = 1; i < argc; ++i) {
    if (ArgMatch(argc, argv, i, "--data") && i + 1 < argc) {
      dataDir = argv[++i];
    } else if (ArgMatch(argc, argv, i, "--out") && i + 1 < argc) {
      outDir = argv[++i];
    } else if (ArgMatch(argc, argv, i, "--list-mpqs")) {
      listOnly = true;
    } else if (ArgMatch(argc, argv, i, "-h") ||
               ArgMatch(argc, argv, i, "--help")) {
      PrintUsage(argv[0]);
      return 0;
    } else {
      std::cerr << "Unknown argument: " << argv[i] << "\n";
      PrintUsage(argv[0]);
      return 2;
    }
  }

  if (dataDir.empty() || (!listOnly && outDir.empty())) {
    PrintUsage(argv[0]);
    return 2;
  }

  if (listOnly) {
    return firelands::extract::RunListMpqsTask(dataDir, std::cout, std::cerr);
  }
  return firelands::extract::RunDbcExtractTask(dataDir, outDir, std::cout,
                                                 std::cerr);
}
