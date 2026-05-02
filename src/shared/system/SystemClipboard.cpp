#include "SystemClipboard.h"

#include <cstddef>
#include <cstdio>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <unistd.h>
#else
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace Firelands {
namespace {

#if !defined(_WIN32) && !defined(__APPLE__)

bool TryPipeToClipboardCommand(char const *cmd, std::string_view data) {
  FILE *pipe = popen(cmd, "w");
  if (pipe == nullptr) {
    return false;
  }
  std::size_t const n = data.size();
  std::size_t written = fwrite(data.data(), 1, n, pipe);
  int const st = pclose(pipe);
  if (written != n) {
    return false;
  }
  if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
    return false;
  }
  return true;
}

#endif // !WIN32 && !APPLE

} // namespace

void SetSystemClipboardUtf8(std::string_view utf8) {
  if (utf8.empty()) {
    return;
  }

#if defined(_WIN32)
  if (!OpenClipboard(nullptr)) {
    return;
  }
  if (!EmptyClipboard()) {
    CloseClipboard();
    return;
  }
  int const wchars =
      MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                          static_cast<int>(utf8.size()), nullptr, 0);
  if (wchars <= 0) {
    CloseClipboard();
    return;
  }
  HGLOBAL const hMem =
      GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(wchars + 1) * sizeof(wchar_t));
  if (hMem == nullptr) {
    CloseClipboard();
    return;
  }
  wchar_t *const locked = static_cast<wchar_t *>(GlobalLock(hMem));
  if (locked == nullptr) {
    GlobalFree(hMem);
    CloseClipboard();
    return;
  }
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                      static_cast<int>(utf8.size()), locked, wchars);
  locked[wchars] = L'\0';
  GlobalUnlock(hMem);
  if (SetClipboardData(CF_UNICODETEXT, hMem) == nullptr) {
    GlobalFree(hMem);
  }
  CloseClipboard();

#elif defined(__APPLE__)
  FILE *pipe = popen("/usr/bin/pbcopy", "w");
  if (pipe == nullptr) {
    return;
  }
  fwrite(utf8.data(), 1, utf8.size(), pipe);
  pclose(pipe);

#else
  // Wayland then X11 (typical Linux setups).
  if (TryPipeToClipboardCommand("wl-copy 2>/dev/null", utf8)) {
    return;
  }
  if (TryPipeToClipboardCommand("xclip -selection clipboard 2>/dev/null", utf8)) {
    return;
  }
  (void)TryPipeToClipboardCommand("xsel --clipboard --input 2>/dev/null", utf8);
#endif
}

} // namespace Firelands
