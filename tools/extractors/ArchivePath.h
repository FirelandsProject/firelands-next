#pragma once

#include <filesystem>
#include <string>

namespace firelands::extract {

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

} // namespace firelands::extract
