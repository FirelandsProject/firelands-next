#pragma once

#include <cstddef>
#include <sstream>
#include <string>

namespace Firelands::gm_gossip {

/// Design tokens for in-game **GM gossip** bodies (`SMSG_NPC_TEXT_UPDATE`).
///
/// Assumed client surface: Cataclysm gossip parchment (~`#D4B896`). Foregrounds are
/// dark/warm ink tones so normal text meets **WCAG AA** (~4.5:1+); titles use
/// deeper hues for **AA on large / bold** treatment. Chat notification palettes
/// (light gray on black) must not be reused here.
///
/// Token map:
/// | Role        | Token        | Hex     | Use |
/// |-------------|--------------|---------|-----|
/// | Page title  | `kTitle`     | 0B3D4A   | Window / page heading |
/// | Section     | `kSection`   | 5C3D2E   | Subheading |
/// | Label       | `kLabel`     | 3A3228   | Field name |
/// | Value       | `kValue`     | 111111   | Primary data |
/// | Accent      | `kAccent`    | 005A9E   | Ids, spells, entry |
/// | Subname     | `kSubname`   | 4A5568   | Secondary name line |
/// | Muted       | `kMuted`     | 5C534A   | Hints, commands |
/// | Rule        | `kRule`      | 6B5E52   | Separator line |
/// | Warning     | `kWarning`   | B91C1C   | Errors / evade |
/// | Success     | `kSuccess`   | 1B6B2E   | OK states |
/// | Chevron     | `kChevron`   | 5C534A   | `>` breadcrumb |
inline constexpr char kReferenceBackgroundHex[] = "D4B896";

namespace color {
inline constexpr char kReset[] = "|r";
inline constexpr char kTitle[] = "|cff0B3D4A";
inline constexpr char kSection[] = "|cff5C3D2E";
inline constexpr char kLabel[] = "|cff3A3228";
inline constexpr char kValue[] = "|cff111111";
inline constexpr char kAccent[] = "|cff005A9E";
inline constexpr char kSubname[] = "|cff4A5568";
inline constexpr char kMuted[] = "|cff5C534A";
inline constexpr char kRule[] = "|cff6B5E52";
inline constexpr char kWarning[] = "|cffB91C1C";
inline constexpr char kSuccess[] = "|cff1B6B2E";
inline constexpr char kChevron[] = "|cff5C534A";
} // namespace color

/// WoW gossip line break (`\n` is ignored; renders as one clipped line).
inline constexpr char kLine[] = "$B";
inline constexpr char kParagraph[] = "$B$B";

inline std::string TruncateBody(std::string const &text, std::size_t maxLen = 1500) {
  if (text.size() <= maxLen)
    return text;
  if (maxLen <= 3)
    return text.substr(0, maxLen);
  return text.substr(0, maxLen - 3) + "...";
}

inline void AppendRule(std::ostringstream &body) {
  body << color::kRule << "----------------------------------------" << color::kReset
       << kLine;
}

inline void AppendPageTitle(std::ostringstream &body, std::string const &title) {
  body << color::kTitle << title << color::kReset << kLine;
}

inline void AppendSectionHeader(std::ostringstream &body, std::string const &title) {
  body << kParagraph << color::kSection << title << color::kReset << kLine;
}

inline void AppendLabelValue(std::ostringstream &body, char const *label,
                             std::string const &value,
                             char const *valueColor = color::kValue) {
  body << color::kLabel << label << ":|r " << valueColor << value << color::kReset
       << kLine;
}

} // namespace Firelands::gm_gossip
