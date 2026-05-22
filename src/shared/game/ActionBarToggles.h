#pragma once

#include <cstdint>

namespace Firelands::ActionBarToggles {

/// Cataclysm 4.3.4 `PLAYER_FIELD_BYTES` action-bar toggle byte (bit = 1 → bar shown).
/// Matches client `SetActionBarToggles(bottomLeft, bottomRight, sideRight, sideRight2, alwaysShow)`.
constexpr uint8_t kBottomLeft = 0x01;
constexpr uint8_t kBottomRight = 0x02;
constexpr uint8_t kRight = 0x04;
constexpr uint8_t kRight2 = 0x08;
constexpr uint8_t kAlwaysShow = 0x10;

constexpr uint8_t kAllExtraBars = kBottomLeft | kBottomRight | kRight | kRight2;
constexpr uint8_t kDefaultVisible = 0xFF;
constexpr uint8_t kAllHidden = 0x00;

} // namespace Firelands::ActionBarToggles
