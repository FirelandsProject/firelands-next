#pragma once

#include <cctype>
#include <filesystem>
#include <string>

namespace firelands::extract {

inline void NormalizeSlashesLowerInPlace(std::string &s) {
  for (char &c : s) {
    if (c == '/') {
      c = '\\';
    }
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
}

// Converts `DBFilesClient\Spell.dbc` style paths to a relative `std::filesystem::path`.
inline std::filesystem::path ArchivedPathToRelative(const std::string &archived) {
  std::filesystem::path out;
  std::string part;
  part.reserve(32);
  for (char c : archived) {
    if (c == '\\' || c == '/') {
      if (!part.empty()) {
        out /= part;
        part.clear();
      }
    } else {
      part.push_back(c);
    }
  }
  if (!part.empty()) {
    out /= part;
  }
  return out;
}

// DBC/DB2 extraction writes under `out/dbc/<tail>` (drops the `DBFilesClient` segment).
// Uses the same path-shape rules as `IsDbFilesClientDataStorePath` in ExtractorTasks.cpp;
// indices are derived from the slash-normalized lower form, which matches `archived`
// byte offsets for ASCII WoW paths (only `/`→`\` and case may differ).
inline std::filesystem::path DbcStoreOutputRelativePath(const std::string &archived) {
  std::string n = archived;
  NormalizeSlashesLowerInPlace(n);
  static const char kPrefix[] = "dbfilesclient\\";
  constexpr std::size_t pl = sizeof(kPrefix) - 1;
  std::size_t tailStart = std::string::npos;
  if (n.size() >= pl && n.compare(0, pl, kPrefix) == 0) {
    tailStart = pl;
  } else {
    static const char kMid[] = "\\dbfilesclient\\";
    constexpr std::size_t ml = sizeof(kMid) - 1;
    std::size_t const p = n.find(kMid);
    if (p != std::string::npos) {
      tailStart = p + ml;
    }
  }
  std::filesystem::path const dbcRoot("dbc");
  if (tailStart == std::string::npos || tailStart >= archived.size()) {
    return dbcRoot / ArchivedPathToRelative(archived).filename();
  }
  return dbcRoot / ArchivedPathToRelative(archived.substr(tailStart));
}

inline std::filesystem::path DbcOutputPath(const std::filesystem::path &outDir,
                                           const std::string &archived) {
  std::filesystem::path relative = DbcStoreOutputRelativePath(archived);
  if (outDir.filename() == "dbc" && !relative.empty() &&
      *relative.begin() == "dbc") {
    relative = relative.lexically_relative("dbc");
  }
  return outDir / relative;
}

} // namespace firelands::extract
