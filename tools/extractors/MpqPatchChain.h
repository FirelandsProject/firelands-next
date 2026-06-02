#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <StormLib.h>

namespace firelands::extract {

class MpqPatchChain {
public:
  MpqPatchChain() = default;
  MpqPatchChain(const MpqPatchChain &) = delete;
  MpqPatchChain &operator=(const MpqPatchChain &) = delete;

  ~MpqPatchChain() { Close(); }

  bool Open(const std::vector<std::filesystem::path> &orderedArchives);
  void Close();

  bool FileExists(const char *archivedPath) const;
  bool ExtractFile(const char *archivedPath,
                     const std::filesystem::path &destPath) const;

  // Enumerates files matching `wildcard` from the patch chain (the open handle).
  // Calls `visitor` for each found name.
  bool ForEachFile(const char *wildcard,
                   const std::function<void(const std::string &)> &visitor) const;

  // Opens each archive in `orderedArchives` INDEPENDENTLY (not as a chain) and
  // collects all file names matching `wildcard`, deduplicating across archives so
  // the LAST (highest-priority) archive's name for a given file wins.
  // Useful when chain-based enumeration misses files that exist only in patch MPQs.
  static bool EnumerateAcrossArchives(
      const std::vector<std::filesystem::path> &orderedArchives,
      const char *wildcard,
      const std::function<void(const std::string &)> &visitor);

  // Extracts from the highest-priority archive containing `archivedPath`.
  // This is useful for clients whose MPQs do not form a StormLib patch chain.
  static bool ExtractFromBestArchive(
      const std::vector<std::filesystem::path> &orderedArchives,
      const char *archivedPath,
      const std::filesystem::path &destPath);

  static bool FileExistsInAnyArchive(
      const std::vector<std::filesystem::path> &orderedArchives,
      const char *archivedPath);

  bool IsOpen() const { return handle_ != nullptr; }

private:
  HANDLE handle_ = nullptr;
};

} // namespace firelands::extract
