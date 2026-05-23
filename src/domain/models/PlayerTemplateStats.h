#pragma once

#include <cstdint>

namespace Firelands {

/// Base primary attributes from `firelands_world.player_classlevelstats` (reference).
struct PlayerClassLevelStats {
  uint16_t str = 0;
  uint16_t agi = 0;
  uint16_t sta = 0;
  uint16_t inte = 0;
  uint16_t spi = 0;
};

/// Racial modifiers from `firelands_world.player_racestats` (signed; summed with class row).
struct PlayerRaceStats {
  int16_t str = 0;
  int16_t agi = 0;
  int16_t sta = 0;
  int16_t inte = 0;
  int16_t spi = 0;
};

} // namespace Firelands
