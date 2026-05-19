#pragma once

#ifndef _WIN32
#include <csignal>
#endif

namespace Firelands {

/// While a fullscreen TUI runs, ignore SIGINT so Ctrl+C does not tear down the
/// alternate screen and dump raw scrollback. Restore the handler on destruction.
struct IgnoreSigIntForTui {
#ifndef _WIN32
  void (*previous_)(int) = SIG_ERR;
  IgnoreSigIntForTui() { previous_ = std::signal(SIGINT, SIG_IGN); }
  ~IgnoreSigIntForTui() {
    if (previous_ != SIG_ERR) {
      std::signal(SIGINT, previous_);
    }
  }
#else
  IgnoreSigIntForTui() = default;
  ~IgnoreSigIntForTui() = default;
#endif
};

} // namespace Firelands
