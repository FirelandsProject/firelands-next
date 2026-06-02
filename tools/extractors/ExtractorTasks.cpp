#include "ExtractorTasks.h"

#include "ArchivePath.h"
#include "KnownDbcFiles.h"
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
  NormalizeSlashesLowerInPlace(s);
  return s;
}

// Internal MPQ paths are usually "DBFilesClient\Foo.dbc" or "...\Item.db2";
// some builds use '/'.
bool IsDbFilesClientDataStorePath(const std::string &archived) {
  std::string n = archived;
  NormalizeSlashesLowerInPlace(n);
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
                    std::ostream &err, const std::string &locale) {
  const auto mpqs = BuildCataclysmMpqOpenOrder(dataDir, locale);
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
                      std::ostream &err, const std::string &locale) {
  const auto mpqs = BuildCataclysmMpqOpenOrder(dataDir, locale);
  if (mpqs.empty()) {
    err << "No .mpq archives found in: " << dataDir.string() << "\n";
    return 1;
  }

  MpqPatchChain chain;
  bool const chainOpen = chain.Open(mpqs);

  // Discover *.dbc and *.db2 under DBFilesClient: enumerate each MPQ, then the
  // open chain as a fallback when StormLib can build one. Some launcher-streamed
  // 4.3.4 clients do not form a valid patch chain, so chain failure is not fatal.
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
    if (chainOpen) {
      chain.ForEachFile(wildcard, [&](const std::string &name) {
        if (!IsDbFilesClientDataStorePath(name)) {
          return;
        }
        const std::string k = NormalizeSlashesLower(name);
        if (seenNorm.insert(k).second) {
          storePaths.push_back(name);
        }
      });
    }
  };

  collectWildcard("DBFilesClient/*.dbc");
  collectWildcard("DBFilesClient/*.db2");

  const size_t wildcardDiscovered = storePaths.size();
  for (const char *fileName : known_dbc::kCataclysmDbcFiles) {
    const std::string archived = std::string("DBFilesClient/") + fileName;
    const std::string k = NormalizeSlashesLower(archived);
    if (seenNorm.find(k) != seenNorm.end()) {
      continue;
    }
    if (MpqPatchChain::FileExistsInAnyArchive(mpqs, archived.c_str())) {
      printf("FOUND: %s\n", archived.c_str()); fflush(stdout);
      seenNorm.insert(k);
      storePaths.push_back(archived);
    } else if (storePaths.empty() && fileName == known_dbc::kCataclysmDbcFiles[0]) {
      printf("FIRST CHECK: %s NOT FOUND\n", archived.c_str()); fflush(stdout);
    }
  }

  if (wildcardDiscovered == 0 && !storePaths.empty()) {
    out << "MPQ listfile unavailable; using known Cataclysm 4.3.4 DBC/DB2 names.\n";
  }

  size_t extracted = 0;
  size_t failed = 0;

  for (const auto &archived : storePaths) {
    const std::filesystem::path dest = DbcOutputPath(outDir, archived);
    bool ok = false;
    if (chainOpen) {
      ok = chain.ExtractFile(archived.c_str(), dest);
    }
    if (!ok) {
      ok = MpqPatchChain::ExtractFromBestArchive(mpqs, archived.c_str(), dest);
    }
    if (ok) {
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

  std::filesystem::path shownOut = outDir;
  if (outDir.filename() != "dbc") {
    shownOut /= "dbc";
  }
  out << "Extracted " << extracted << " DBFilesClient file(s) (.dbc / .db2) under "
      << shownOut.string() << "\n";
  return failed != 0 ? 1 : 0;
}

int RunMapExtractTask(const std::filesystem::path &dataDir,
                      const std::filesystem::path &outDir, std::ostream &out,
                      std::ostream &err, const std::string &locale) {
  const auto mpqs = BuildCataclysmMpqOpenOrder(dataDir, locale);
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
