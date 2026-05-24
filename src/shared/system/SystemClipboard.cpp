#include "SystemClipboard.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#else
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace Firelands {
namespace {

#if !defined(_WIN32)

bool TryPipeFromClipboardCommand(char const *cmd, std::string *out) {
  FILE *pipe = popen(cmd, "r");
  if (pipe == nullptr) {
    return false;
  }
  std::string buf;
  char chunk[4096];
  while (true) {
    std::size_t const n = fread(chunk, 1, sizeof(chunk), pipe);
    if (n > 0) {
      buf.append(chunk, n);
    }
    if (n < sizeof(chunk)) {
      break;
    }
  }
  int const st = pclose(pipe);
  if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
    return false;
  }
  if (!buf.empty() && buf.back() == '\n') {
    buf.pop_back();
  }
  *out = std::move(buf);
  return true;
}

#if !defined(__APPLE__)

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

#endif // !APPLE

#endif // !WIN32

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

std::optional<std::string> GetSystemClipboardUtf8() {
#if defined(_WIN32)
  if (!OpenClipboard(nullptr)) {
    return std::nullopt;
  }

  auto read_unicode = []() -> std::optional<std::string> {
    HANDLE const h = GetClipboardData(CF_UNICODETEXT);
    if (h == nullptr) {
      return std::nullopt;
    }
    wchar_t const *const w = static_cast<wchar_t const *>(GlobalLock(h));
    if (w == nullptr) {
      return std::nullopt;
    }
    int const bytes =
        WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1) {
      GlobalUnlock(h);
      return std::nullopt;
    }
    std::vector<char> utf8(static_cast<std::size_t>(bytes - 1));
    WideCharToMultiByte(CP_UTF8, 0, w, -1, utf8.data(), bytes, nullptr, nullptr);
    GlobalUnlock(h);
    return std::string(utf8.begin(), utf8.end());
  };

  std::optional<std::string> result = read_unicode();
  CloseClipboard();
  if (!result || result->empty()) {
    return std::nullopt;
  }
  return result;

#elif defined(__APPLE__)
  std::string out;
  if (!TryPipeFromClipboardCommand("/usr/bin/pbpaste", &out) || out.empty()) {
    return std::nullopt;
  }
  return out;

#else
  std::string out;
  if (TryPipeFromClipboardCommand("wl-paste -n 2>/dev/null", &out) && !out.empty()) {
    return out;
  }
  if (TryPipeFromClipboardCommand("xclip -selection clipboard -o 2>/dev/null", &out) &&
      !out.empty()) {
    return out;
  }
  if (TryPipeFromClipboardCommand("xsel --clipboard --output 2>/dev/null", &out) &&
      !out.empty()) {
    return out;
  }
  return std::nullopt;
#endif
}

} // namespace Firelands
