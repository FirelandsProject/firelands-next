#ifndef FIRELANDS_SHARED_BANNER_H
#define FIRELANDS_SHARED_BANNER_H

#include <cstdlib>
#include <iostream>
#include <string>

#ifndef _WIN32
#include <unistd.h>
#else
#include <cstdio>
#include <io.h>
#endif

namespace Firelands {

enum class BannerType { Auth, World, Tools };

/**
 * Sticky header layout (DECSTBM scrolling region): keeps the banner on screen
 * while new logs scroll underneath. Terminal mouse-wheel scrolling typically
 * moves scrollback, not that inner region, so reviewing older logs becomes
 * awkward and lines that leave the region may feel “lost”.
 *
 * Default comes from YAML (`Log.StickyBanner` on world/auth). Optional override:
 *   FIRELANDS_CONSOLE_STICKY_BANNER=1   force sticky on
 *   FIRELANDS_CONSOLE_STICKY_BANNER=0   force sticky off
 * Any other non-empty value falls back to the YAML flag.
 */
inline bool ResolveStickyBanner(bool yamlStickyBanner) {
  const char *v = std::getenv("FIRELANDS_CONSOLE_STICKY_BANNER");
  if (v == nullptr || v[0] == '\0')
    return yamlStickyBanner;
  if (v[0] == '1' && v[1] == '\0')
    return true;
  if (v[0] == '0' && v[1] == '\0')
    return false;
  return yamlStickyBanner;
}

inline bool StdoutIsInteractiveTerminal() {
#ifndef _WIN32
  return ::isatty(STDOUT_FILENO) != 0;
#else
  return ::_isatty(::_fileno(stdout)) != 0;
#endif
}

/**
 * @brief Prints a cool ASCII art banner for the Firelands project.
 * Uses ANSI escape codes for coloring if supported.
 *
 * @param type The type of banner to print.
 * @param fixed If true (and stdout is a TTY), clears the screen and sets a
 * scrolling region below the banner. Prefer false for normal scrollback /
 * mouse-wheel behaviour.
 */
inline void PrintBanner(BannerType type = BannerType::Auth,
                        bool fixed = false) {
  const bool useStickyLayout = fixed && StdoutIsInteractiveTerminal();

  // ANSI Colors - Intense Orange Palette
  const std::string ORANGE_VIBRANT = "\033[38;5;202m";
  const std::string ORANGE_BRIGHT = "\033[38;5;208m";
  const std::string ORANGE_WARM = "\033[38;5;214m";
  const std::string BLUE_HIGHLIGHT =
      "\033[38;5;39m"; // Complementary Deep Sky Blue
  const std::string WHITE = "\033[37m";
  const std::string RESET = "\033[0m";
  const std::string BOLD = "\033[1m";
  const std::string CLEAR_SCREEN = "\033[2J";
  const std::string CURSOR_HOME = "\033[H";
  const std::string GRAY = "\033[90m";

  if (useStickyLayout) {
    // Full-screen reset, clear visible screen only (do not wipe scrollback —
    // \033[3J breaks wheel-based history). Then draw banner from home.
    std::cout << "\033[r" << CLEAR_SCREEN << CURSOR_HOME << std::flush;
  }

  std::string mainColor = ORANGE_BRIGHT;
  std::string label = " PROJECT INTERNALS ";

  switch (type) {
  case BannerType::Auth:
    mainColor = ORANGE_VIBRANT;
    label = " AUTH SERVER ";
    break;
  case BannerType::World:
    mainColor = ORANGE_BRIGHT;
    label = " WORLD SERVER ";
    break;
  case BannerType::Tools:
    mainColor = ORANGE_WARM;
    label = " DEVELOPER TOOLS ";
    break;
  }

  std::cout << mainColor << BOLD << R"(
    ███████╗██╗██████╗ ███████╗██╗      █████╗ ███╗   ██╗██████╗ ███████╗
    ██╔════╝██║██╔══██╗██╔════╝██║     ██╔══██╗████╗  ██║██╔══██╗██╔════╝
    █████╗  ██║██████╔╝█████╗  ██║     ███████║██╔██╗ ██║██║  ██║███████╗
    ██╔══╝  ██║██╔══██╗██╔══╝  ██║     ██╔══██║██║╚██╗██║██║  ██║╚════██║
    ██║     ██║██║  ██║███████╗███████╗██║  ██║██║ ╚████║██████╔╝███████║
    ╚═╝     ╚═╝╚═╝  ╚═╝╚══════╝╚══════╝╚═╝  ╚═╝╚═╝  ╚═══╝╚═════╝ ╚══════╝
        )" << WHITE
            << "           Cataclysm WoW Emulator | " << BOLD << BLUE_HIGHLIGHT
            << label << RESET << WHITE << " | Build 15595" << RESET << "\n";

  std::cout << GRAY << "    " << std::string(72, '-') << RESET << std::endl;

  if (useStickyLayout) {
    // DECSTBM: scroll region from first log line to bottom (999 clamps to height).
    std::cout << "\033[11;999r" << "\033[11;1H" << std::flush;
  }
}

} // namespace Firelands

#endif // FIRELANDS_SHARED_BANNER_H
