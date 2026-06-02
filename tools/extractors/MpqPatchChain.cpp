#include "MpqPatchChain.h"

namespace firelands::extract {

bool MpqPatchChain::Open(const std::vector<std::filesystem::path> &orderedArchives) {
  Close();
  if (orderedArchives.empty()) {
    return false;
  }

  if (!SFileOpenArchive(orderedArchives.front().c_str(), 0, MPQ_FLAG_READ_ONLY, &handle_)) {
    handle_ = nullptr;
    return false;
  }

  for (size_t i = 1; i < orderedArchives.size(); ++i) {
    if (!SFileOpenPatchArchive(handle_, orderedArchives[i].c_str(), nullptr, 0)) {
      Close();
      return false;
    }
  }
  return true;
}

void MpqPatchChain::Close() {
  if (handle_) {
    SFileCloseArchive(handle_);
    handle_ = nullptr;
  }
}

bool MpqPatchChain::FileExists(const char *archivedPath) const {
  if (!handle_) {
    return false;
  }
  HANDLE file = nullptr;
  if (!SFileOpenFileEx(handle_, archivedPath, 0, &file)) {
    return false;
  }
  SFileCloseFile(file);
  return true;
}

bool MpqPatchChain::ExtractFile(const char *archivedPath,
                                const std::filesystem::path &destPath) const {
  if (!handle_) {
    return false;
  }
  std::filesystem::create_directories(destPath.parent_path());
#if defined(_WIN32)
  const std::wstring wdest = destPath.wstring();
  return SFileExtractFile(handle_, archivedPath, wdest.c_str(), SFILE_OPEN_FROM_MPQ);
#else
  const std::string dest = destPath.string();
  return SFileExtractFile(handle_, archivedPath, dest.c_str(), SFILE_OPEN_FROM_MPQ);
#endif
}

bool MpqPatchChain::ForEachFile(
    const char *wildcard,
    const std::function<void(const std::string &)> &visitor) const {
  if (!handle_) {
    return false;
  }
  SFILE_FIND_DATA findData{};
  HANDLE find = SFileFindFirstFile(handle_, wildcard, &findData, nullptr);
  if (find == nullptr) {
    // NULL = no files matched (ERROR_NO_MORE_FILES) or a real error; either way not fatal.
    return true;
  }
  do {
    visitor(std::string(findData.cFileName));
  } while (SFileFindNextFile(find, &findData));
  SFileFindClose(find);
  return true;
}

// static
bool MpqPatchChain::EnumerateAcrossArchives(
    const std::vector<std::filesystem::path> &orderedArchives,
    const char *wildcard,
    const std::function<void(const std::string &)> &visitor) {
  // Map normalised-lower name → original-case name from the highest-priority archive.
  // We process archives in order (lowest → highest priority), overwriting earlier
  // entries, so the final map holds the highest-priority version of each file name.
  std::unordered_map<std::string, std::string> collected; // lower → original

  for (const auto &archivePath : orderedArchives) {
    HANDLE h = nullptr;
    if (!SFileOpenArchive(archivePath.c_str(), 0, MPQ_FLAG_READ_ONLY, &h) || h == nullptr) {
      continue;
    }

    SFILE_FIND_DATA fd{};
    HANDLE find = SFileFindFirstFile(h, wildcard, &fd, nullptr);
    if (find != nullptr) {
      do {
        const std::string orig(fd.cFileName);
        std::string lower = orig;
        for (char &c : lower) {
          c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        // Replace / with \ so deduplication is separator-agnostic.
        for (char &c : lower) {
          if (c == '/') {
            c = '\\';
          }
        }
        collected[lower] = orig;
      } while (SFileFindNextFile(find, &fd));
      SFileFindClose(find);
    }
    SFileCloseArchive(h);
  }

  for (const auto &kv : collected) {
    visitor(kv.second);
  }
  return true;
}

// static
bool MpqPatchChain::ExtractFromBestArchive(
    const std::vector<std::filesystem::path> &orderedArchives,
    const char *archivedPath,
    const std::filesystem::path &destPath) {
  std::filesystem::create_directories(destPath.parent_path());

  for (auto it = orderedArchives.rbegin(); it != orderedArchives.rend(); ++it) {
    HANDLE h = nullptr;
    if (!SFileOpenArchive(it->c_str(), 0, MPQ_FLAG_READ_ONLY, &h) || h == nullptr) {
      continue;
    }

    HANDLE file = nullptr;
    bool const exists =
        SFileOpenFileEx(h, archivedPath, SFILE_OPEN_FROM_MPQ, &file);
    if (exists && file) {
      SFileCloseFile(file);
#if defined(_WIN32)
      const std::wstring wdest = destPath.wstring();
      bool const ok = SFileExtractFile(h, archivedPath, wdest.c_str(),
                                       SFILE_OPEN_FROM_MPQ);
#else
      const std::string dest = destPath.string();
      bool const ok = SFileExtractFile(h, archivedPath, dest.c_str(),
                                       SFILE_OPEN_FROM_MPQ);
#endif
      SFileCloseArchive(h);
      return ok;
    }

    if (file) {
      SFileCloseFile(file);
    }
    SFileCloseArchive(h);
  }

  return false;
}

// static
bool MpqPatchChain::FileExistsInAnyArchive(
    const std::vector<std::filesystem::path> &orderedArchives,
    const char *archivedPath) {
  static int callCount = 0;
  callCount++;
  for (auto it = orderedArchives.rbegin(); it != orderedArchives.rend(); ++it) {
    HANDLE h = nullptr;
    if (!SFileOpenArchive(it->c_str(), 0, MPQ_FLAG_READ_ONLY, &h) || h == nullptr) {
      if (callCount == 1) printf("OPEN_FAIL: %s\n", it->string().c_str());
      continue;
    }

    HANDLE file = nullptr;
    bool const exists =
        SFileOpenFileEx(h, archivedPath, SFILE_OPEN_FROM_MPQ, &file);
    if (callCount == 1) printf("  %s -> %s = %d\n", it->filename().string().c_str(), archivedPath, (int)exists);
    if (file) {
      SFileCloseFile(file);
    }
    SFileCloseArchive(h);

    if (exists) {
      return true;
    }
  }

  return false;
}

} // namespace firelands::extract
