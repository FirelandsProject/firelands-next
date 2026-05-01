#include "ExtractorInteractive.h"

#include "ExtractorTasks.h"

#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string>

namespace firelands::extract {
namespace {

namespace fs = std::filesystem;

std::string Trim(std::string s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
    s.erase(s.begin());
  }
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
    s.pop_back();
  }
  return s;
}

std::string ReadLine() {
  std::string line;
  if (!std::getline(std::cin, line)) {
    return {};
  }
  return Trim(line);
}

void ClearInputFail() {
  if (std::cin.fail()) {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  }
}

void PrintBanner(const char *programName) {
  std::cout
      << "\n"
      << "  Firelands — client data extractors (Cataclysm / MPQ)\n"
      << "  -----------------------------------------------\n"
      << "  Program: " << programName << "\n\n"
      << "  Non-interactive mode (scripts / CI):\n"
      << "    firelands-dbc-extractor  --data <WoW/Data> --out <dir>\n"
      << "    firelands-map-extractor  --data <WoW/Data> --out <dir>\n"
      << "    (add --list-mpqs to print MPQ open order only)\n\n";
}

void PrintMainMenu() {
  std::cout << "  Choose an action:\n"
            << "    1  Extract DBC files (DBFilesClient\\*.dbc)\n"
            << "    2  Extract map assets (World\\maps\\ … .adt / .wdt / .wdl)\n"
            << "    3  List MPQ open order only (no extraction)\n"
            << "    4  Exit\n"
            << "\n"
            << "  Enter choice [1-4]: " << std::flush;
}

int ReadMenuChoice() {
  int n = 0;
  if (!(std::cin >> n)) {
    ClearInputFail();
    return -1;
  }
  if (std::cin.peek() != '\n' && std::cin.peek() != EOF) {
    ClearInputFail();
    return -1;
  }
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  return n;
}

std::optional<fs::path> PromptDataDirectory() {
  bool first = true;
  for (;;) {
    std::cout << "\n  WoW client Data folder path (contains .MPQ files):\n";
    if (first) {
      std::cout << "  (Press Enter alone to cancel.)\n";
    } else {
      std::cout << "  (Press Enter alone to cancel, or enter a valid directory.)\n";
    }
    std::cout << "  > " << std::flush;
    first = false;
    const std::string line = ReadLine();
    if (line.empty()) {
      std::cout << "  (Cancelled.)\n";
      return std::nullopt;
    }
    fs::path p(line);
    if (!fs::exists(p) || !fs::is_directory(p)) {
      std::cout << "  That path is not an existing directory.\n";
      continue;
    }
    return p;
  }
}

std::optional<fs::path> PromptOutputDirectory() {
  std::cout << "\n  Output folder (will be created if missing):\n"
            << "  (Press Enter alone to cancel.)\n"
            << "  > " << std::flush;
  const std::string line = ReadLine();
  if (line.empty()) {
    std::cout << "  (Cancelled.)\n";
    return std::nullopt;
  }
  return fs::path(line);
}

void PauseReturn() {
  std::cout << "\n  Press Enter to return to the menu..." << std::flush;
  ReadLine();
}

} // namespace

int RunInteractiveMenu(const char *programName) {
  PrintBanner(programName);

  for (;;) {
    PrintMainMenu();
    const int choice = ReadMenuChoice();
    if (choice < 1 || choice > 4) {
      std::cout << "  Invalid choice. Please enter 1, 2, 3, or 4.\n";
      PauseReturn();
      continue;
    }
    if (choice == 4) {
      std::cout << "\n  Goodbye.\n\n";
      return 0;
    }

    const auto dataDir = PromptDataDirectory();
    if (!dataDir) {
      PauseReturn();
      continue;
    }

    if (choice == 3) {
      std::cout << "\n  MPQ open order (base first, then patch overlays):\n\n";
      const int rc =
          RunListMpqsTask(*dataDir, std::cout, std::cerr);
      if (rc != 0) {
        PauseReturn();
        continue;
      }
      PauseReturn();
      continue;
    }

    const auto outDir = PromptOutputDirectory();
    if (!outDir) {
      PauseReturn();
      continue;
    }

    std::cout << "\n  Working…\n\n";
    int rc = 0;
    if (choice == 1) {
      rc = RunDbcExtractTask(*dataDir, *outDir, std::cout, std::cerr);
    } else {
      rc = RunMapExtractTask(*dataDir, *outDir, std::cout, std::cerr);
    }

    if (rc != 0) {
      std::cout << "\n  Finished with errors (see messages above).\n";
    } else {
      std::cout << "\n  Done.\n";
    }
    PauseReturn();
  }
}

} // namespace firelands::extract
