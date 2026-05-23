#include "SystemBeep.h"

#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace Firelands {

void PlaySystemBeep() {
#if defined(_WIN32)
  MessageBeep(MB_OK);
#elif defined(__APPLE__)
  if (std::system("/usr/bin/osascript -e beep 2>/dev/null") != 0) {
    std::fputc('\a', stderr);
    std::fflush(stderr);
  }
#else
  std::fputc('\a', stderr);
  std::fflush(stderr);
#endif
}

} // namespace Firelands
