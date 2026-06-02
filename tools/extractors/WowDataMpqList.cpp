#include "WowDataMpqList.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <regex>
#include <unordered_map>

namespace firelands::extract {
namespace {

std::string ToLower(std::string s) {
  for (char &c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

bool EndsWithMpqExtension(const std::string &lowerName) {
  return lowerName.size() >= 4 &&
         lowerName.compare(lowerName.size() - 4, 4, ".mpq") == 0;
}

bool LooksLikeLocaleName(const std::string &name) {
  return name.size() == 4 &&
         std::isalpha(static_cast<unsigned char>(name[0])) &&
         std::isalpha(static_cast<unsigned char>(name[1])) &&
         std::isalpha(static_cast<unsigned char>(name[2])) &&
         std::isalpha(static_cast<unsigned char>(name[3]));
}

bool HasLocaleArchive(const std::filesystem::path &dir, const std::string &locale) {
  return std::filesystem::exists(dir / ("locale-" + locale + ".MPQ")) ||
         std::filesystem::exists(dir / ("locale-" + locale + ".mpq"));
}

// Cataclysm 4.3.4 MPQ priority tiers (lower rank = opened first / lower overlay).
// Reference: reference implementation 4.3.4 extractor load order.

// Tier 0: base game data archives (opened first as lowest priority).
constexpr const char *kTier0[] = {
    "common.mpq",
    "common-2.mpq",
    "base-win.mpq",
    "base-osx.mpq",
};
// Tier 1: expansion content archives.
constexpr const char *kTier1[] = {
    "expansion.mpq",
    "lichking.mpq",
    "expansion1.mpq",
    "expansion2.mpq",
    "expansion3.mpq",
};
// Tier 2: world geometry, art, misc — opened after expansions.
constexpr const char *kTier2[] = {
    "art.mpq",
    "interface.mpq",
    "item.mpq",
    "misc.mpq",
    "model.mpq",
    "oldworld.mpq",
    "sound.mpq",
    "texture.mpq",
    "video.mpq",
    "world.mpq",
    "world2.mpq",
};

// Locale subdirectory names used by Blizzard (cover all retail languages).
constexpr const char *kKnownLocaleSubdirs[] = {
    "enUS", "enGB", "esES", "esMX", "frFR", "deDE",
    "koKR", "ptBR", "ptPT", "ruRU", "zhCN", "zhTW",
    "itIT", "plPL",
};

int KnownFileRank(const std::string &lowerBase) {
  static const std::unordered_map<std::string, int> kExact = [] {
    std::unordered_map<std::string, int> m;
    int r = 0;
    for (const char *p : kTier0) {
      m[p] = r++;
    }
    for (const char *p : kTier1) {
      m[p] = r++;
    }
    for (const char *p : kTier2) {
      m[p] = r++;
    }
    return m;
  }();

  auto it = kExact.find(lowerBase);
  if (it != kExact.end()) {
    return it->second;
  }

  // wow-update-base-<build>.MPQ — after static tiers, ordered by build number.
  static const std::regex kWowUpdateBase(
      R"(^wow-update-base-(\d+)\.mpq$)", std::regex::icase);
  std::smatch sm;
  if (std::regex_match(lowerBase, sm, kWowUpdateBase)) {
    return 100000 + std::stoi(sm[1].str());
  }

  static const std::regex kPatchBase(
      R"(^patch-base-(\d+)\.mpq$)", std::regex::icase);
  if (std::regex_match(lowerBase, sm, kPatchBase)) {
    return 110000 + std::stoi(sm[1].str());
  }

  // patch-N.MPQ (patch.MPQ = patch-1).
  static const std::regex kPatchNum(R"(^patch(?:-(\d+))?\.mpq$)", std::regex::icase);
  if (std::regex_match(lowerBase, sm, kPatchNum)) {
    return 200000 + (sm[1].matched ? std::stoi(sm[1].str()) : 1);
  }

  // expansion<N>-locale-<lang>.MPQ
  static const std::regex kExpLocale(
      R"(^expansion\d+-locale-[^.]+\.mpq$)", std::regex::icase);
  if (std::regex_match(lowerBase, sm, kExpLocale)) {
    return 800000;
  }

  // locale-<lang>.MPQ (root locale archives)
  static const std::regex kLocale(R"(^locale-[^.]+\.mpq$)", std::regex::icase);
  if (std::regex_match(lowerBase, sm, kLocale)) {
    return 850000;
  }

  // wow-update-<build>-<lang>.MPQ (locale update patches, highest priority)
  static const std::regex kWowUpdateLocale(
      R"(^wow-update-(\d+)-[^.]+\.mpq$)", std::regex::icase);
  if (std::regex_match(lowerBase, sm, kWowUpdateLocale)) {
    return 900000 + std::stoi(sm[1].str());
  }

  static const std::regex kCacheLocalePatch(
      R"(^patch-[^-]+-(\d+)\.mpq$)", std::regex::icase);
  if (std::regex_match(lowerBase, sm, kCacheLocalePatch)) {
    return 910000 + std::stoi(sm[1].str());
  }

  // All other unrecognised names (mid priority, stable lexical tie-break).
  return 400000;
}

struct MpqEntry {
  std::filesystem::path path;
  std::string lowerBase;
  int rank;
};

bool CompareMpqEntries(const MpqEntry &a, const MpqEntry &b) {
  if (a.rank != b.rank) {
    return a.rank < b.rank;
  }
  return a.lowerBase < b.lowerBase;
}

void CollectMpqsFromDir(const std::filesystem::path &dir,
                        std::vector<MpqEntry> &entries) {
  if (!std::filesystem::is_directory(dir)) {
    return;
  }
  for (const std::filesystem::directory_entry &de :
       std::filesystem::directory_iterator(dir)) {
    if (!de.is_regular_file()) {
      continue;
    }
    const std::string lower = ToLower(de.path().filename().string());
    if (!EndsWithMpqExtension(lower)) {
      continue;
    }
    MpqEntry e;
    e.path = de.path();
    e.lowerBase = lower;
    e.rank = KnownFileRank(lower);
    entries.push_back(std::move(e));
  }
}

std::optional<std::string> ReadLocaleFromConfig(const std::filesystem::path &dataDir) {
  const std::filesystem::path config = dataDir.parent_path() / "WTF" / "Config.wtf";
  std::ifstream in(config);
  if (!in) {
    return std::nullopt;
  }

  static const std::regex kLocaleLine(
      "^\\s*SET\\s+(?:locale|installLocale)\\s+\"([^\"]+)\"",
      std::regex::icase);
  std::string line;
  std::smatch sm;
  while (std::getline(in, line)) {
    if (std::regex_search(line, sm, kLocaleLine)) {
      return sm[1].str();
    }
  }
  return std::nullopt;
}

std::optional<std::string> DetectLocaleFromDataDir(const std::filesystem::path &dataDir) {
  // Prefer real locale roots directly under Data/. Cache folders can contain
  // stale or partial languages and should not decide the client locale.
  for (const char *locale : kKnownLocaleSubdirs) {
    if (HasLocaleArchive(dataDir / locale, locale)) {
      return locale;
    }
  }

  for (const std::filesystem::directory_entry &de :
       std::filesystem::directory_iterator(dataDir)) {
    if (!de.is_directory()) {
      continue;
    }

    const std::string dirName = de.path().filename().string();
    if (!LooksLikeLocaleName(dirName)) {
      continue;
    }

    if (HasLocaleArchive(de.path(), dirName)) {
      return dirName;
    }
  }

  return std::nullopt;
}

std::optional<std::string> DetectClientLocale(const std::filesystem::path &dataDir,
                                              const std::string &preferredLocale) {
  if (!preferredLocale.empty()) {
    return preferredLocale;
  }
  if (auto fromConfig = ReadLocaleFromConfig(dataDir)) {
    return fromConfig;
  }
  return DetectLocaleFromDataDir(dataDir);
}

std::filesystem::path ResolveDataDir(const std::filesystem::path &inputDir) {
  if (std::filesystem::is_directory(inputDir / "Data")) {
    return inputDir / "Data";
  }
  return inputDir;
}

} // namespace

std::vector<std::filesystem::path>
BuildCataclysmMpqOpenOrder(const std::filesystem::path &inputDir,
                           const std::string &preferredLocale) {
  const std::filesystem::path dataDir = ResolveDataDir(inputDir);
  std::vector<MpqEntry> entries;

  // Root Data/ archives.
  CollectMpqsFromDir(dataDir, entries);
  CollectMpqsFromDir(dataDir / "Cache", entries);

  const std::optional<std::string> detectedLocale =
      DetectClientLocale(dataDir, preferredLocale);
  if (detectedLocale) {
    CollectMpqsFromDir(dataDir / *detectedLocale, entries);
    CollectMpqsFromDir(dataDir / "Cache" / *detectedLocale, entries);
  } else {
    // Locale subdirectories (Data/enUS/, Data/frFR/, etc.).
    for (const char *sub : kKnownLocaleSubdirs) {
      CollectMpqsFromDir(dataDir / sub, entries);
      CollectMpqsFromDir(dataDir / "Cache" / sub, entries);
    }
    // Also pick up any non-standard locale subfolders (2-4 char names).
    for (const std::filesystem::directory_entry &de :
         std::filesystem::directory_iterator(dataDir)) {
      if (!de.is_directory()) {
        continue;
      }
      const std::string name = de.path().filename().string();
      if (name.size() < 2 || name.size() > 5) {
        continue;
      }
      CollectMpqsFromDir(de.path(), entries);
      CollectMpqsFromDir(dataDir / "Cache" / name, entries);
    }
  }

  std::sort(entries.begin(), entries.end(), CompareMpqEntries);

  // Deduplicate by canonical lower path (locale scan may double-add via known list).
  std::vector<std::filesystem::path> out;
  out.reserve(entries.size());
  std::unordered_map<std::string, bool> seen;
  for (const MpqEntry &e : entries) {
    const std::string canon = ToLower(e.path.string());
    if (seen.emplace(canon, true).second) {
      out.push_back(e.path);
    }
  }
  return out;
}

} // namespace firelands::extract
