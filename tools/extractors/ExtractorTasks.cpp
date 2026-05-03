#include "ExtractorTasks.h"

#include "ArchivePath.h"
#include "MpqPatchChain.h"
#include "WowDataMpqList.h"

#include <cctype>
#include <cstring>
#include <iostream>
#include <unordered_set>
#include <vector>

namespace firelands::extract {
namespace {

bool EndsWithInsensitive(const std::string &s, const char *ext) {
  const size_t n = s.size();
  const size_t el = std::strlen(ext);
  if (n < el) {
    return false;
  }
  for (size_t i = 0; i < el; ++i) {
    if (std::tolower(static_cast<unsigned char>(s[n - el + i])) !=
        std::tolower(static_cast<unsigned char>(ext[i]))) {
      return false;
    }
  }
  return true;
}

std::string NormalizeSlashesLower(std::string s) {
  for (char &c : s) {
    if (c == '/') {
      c = '\\';
    }
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

// Internal MPQ paths are usually "DBFilesClient\Foo.dbc" or "...\Item.db2";
// some builds use '/'.
bool IsDbFilesClientDataStorePath(const std::string &archived) {
  const std::string n = NormalizeSlashesLower(archived);
  static const char kPrefix[] = "dbfilesclient\\";
  const size_t pl = sizeof(kPrefix) - 1;
  const bool underDbFilesClient =
      (n.size() >= pl && n.compare(0, pl, kPrefix) == 0) ||
      (n.find("\\dbfilesclient\\") != std::string::npos);
  if (!underDbFilesClient) {
    return false;
  }
  return EndsWithInsensitive(n, ".dbc") || EndsWithInsensitive(n, ".db2");
}

bool IsMapAsset(const std::string &archivedNorm) {
  static const char kPrefix[] = "world\\maps\\";
  const size_t pl = sizeof(kPrefix) - 1;
  if (archivedNorm.size() < pl || archivedNorm.compare(0, pl, kPrefix) != 0) {
    return false;
  }
  return EndsWithInsensitive(archivedNorm, ".adt") ||
         EndsWithInsensitive(archivedNorm, ".wdt") ||
         EndsWithInsensitive(archivedNorm, ".wdl");
}

} // namespace

int RunListMpqsTask(const std::filesystem::path &dataDir, std::ostream &out,
                    std::ostream &err) {
  const auto mpqs = BuildCataclysmMpqOpenOrder(dataDir);
  if (mpqs.empty()) {
    err << "No .mpq archives found in: " << dataDir.string() << "\n";
    return 1;
  }
  for (const auto &p : mpqs) {
    out << p.string() << "\n";
  }
  return 0;
}

int RunDbcExtractTask(const std::filesystem::path &dataDir,
                      const std::filesystem::path &outDir, std::ostream &out,
                      std::ostream &err) {
  const auto mpqs = BuildCataclysmMpqOpenOrder(dataDir);
  if (mpqs.empty()) {
    err << "No .mpq archives found in: " << dataDir.string() << "\n";
    return 1;
  }

  MpqPatchChain chain;
  if (!chain.Open(mpqs)) {
    err << "Failed to open MPQ patch chain (first archive: "
        << mpqs.front().string() << ")\n";
    return 1;
  }

  // Discover *.dbc and *.db2 under DBFilesClient: enumerate each MPQ, then the
  // open chain as a fallback (robust against chain gaps on macOS/Linux).
  std::vector<std::string> storePaths;
  std::unordered_set<std::string> seenNorm;

  auto collectWildcard = [&](const char *wildcard) {
    MpqPatchChain::EnumerateAcrossArchives(
        mpqs, wildcard, [&](const std::string &name) {
          if (!IsDbFilesClientDataStorePath(name)) {
            return;
          }
          const std::string k = NormalizeSlashesLower(name);
          if (seenNorm.insert(k).second) {
            storePaths.push_back(name);
          }
        });
    chain.ForEachFile(wildcard, [&](const std::string &name) {
      if (!IsDbFilesClientDataStorePath(name)) {
        return;
      }
      const std::string k = NormalizeSlashesLower(name);
      if (seenNorm.insert(k).second) {
        storePaths.push_back(name);
      }
    });
  };

  collectWildcard("*.dbc");
  collectWildcard("*.db2");

  size_t extracted = 0;
  size_t failed = 0;

  for (const auto &archived : storePaths) {
    const std::filesystem::path dest = outDir / ArchivedPathToRelative(archived);
    if (chain.ExtractFile(archived.c_str(), dest)) {
      ++extracted;
    } else {
      err << "Extract failed: " << archived << "\n";
      ++failed;
    }
  }

  if (extracted == 0 && failed == 0) {
    err << "No DBFilesClient DBC/DB2 files found. Tips:\n"
           "  - --data must be the WoW client \"Data\" folder containing .MPQ files.\n"
           "  - For Cataclysm 4.3.4, locale tables often live in Data/enUS/ (or your locale).\n"
           "  - Use menu option 3 (or --list-mpqs) to see which archives were detected.\n";
  }

  out << "Extracted " << extracted << " DBFilesClient file(s) (.dbc / .db2) to "
      << outDir.string() << "\n";
  return failed != 0 ? 1 : 0;
}

int RunMapExtractTask(const std::filesystem::path &dataDir,
                      const std::filesystem::path &outDir, std::ostream &out,
                      std::ostream &err) {
  const auto mpqs = BuildCataclysmMpqOpenOrder(dataDir);
  if (mpqs.empty()) {
    err << "No .mpq archives found in: " << dataDir.string() << "\n";
    return 1;
  }

  MpqPatchChain chain;
  if (!chain.Open(mpqs)) {
    err << "Failed to open MPQ patch chain (first archive: "
        << mpqs.front().string() << ")\n";
    return 1;
  }

  // Discover map asset names across all archives independently.
  std::unordered_set<std::string> seenNorm;
  std::vector<std::string> mapNames;
  auto collectMap = [&](const std::string &archived) {
    const std::string norm = NormalizeSlashesLower(archived);
    if (IsMapAsset(norm) && seenNorm.insert(norm).second) {
      mapNames.push_back(archived);
    }
  };

  MpqPatchChain::EnumerateAcrossArchives(mpqs, "World\\maps\\*", collectMap);
  MpqPatchChain::EnumerateAcrossArchives(mpqs, "World/maps/*", collectMap);
  // Fallback: chain-based enumeration.
  chain.ForEachFile("World\\maps\\*", collectMap);
  chain.ForEachFile("World/maps/*", collectMap);

  size_t extracted = 0;
  size_t failed = 0;
  for (const auto &archived : mapNames) {
    const std::filesystem::path dest = outDir / ArchivedPathToRelative(archived);
    if (chain.ExtractFile(archived.c_str(), dest)) {
      ++extracted;
    } else {
      err << "Extract failed: " << archived << "\n";
      ++failed;
    }
  }

  const bool ok = true;

  (void)ok;

  if (extracted == 0 && failed == 0) {
    err << "No map files found. Confirm --data is the WoW client \"Data\" folder "
           "and that World/maps/ data exists in MPQs (try --list-mpqs).\n";
  }

  out << "Extracted " << extracted << " map asset file(s) to " << outDir.string()
      << "\n";
  return failed != 0 ? 1 : 0;
}

} // namespace firelands::extract
