#pragma once

#include <cstdint>

namespace Firelands {

/// When `ChrRaces.dbc` is missing, use generic Horde (2) vs Alliance (1) player
/// `FactionTemplate.dbc` ids — always present on the client (avoids WowError from
/// wrong hardcoded race-specific templates).
inline uint32_t SafePlayerFactionTemplateWithoutChrRaces(uint8_t race) {
  switch (race) {
  case 2:  // Orc
  case 5:  // Undead
  case 6:  // Tauren
  case 8:  // Troll
  case 9:  // Goblin
  case 10: // Blood Elf
    return 2u;
  default:
    return 1u;
  }
}

/// Cataclysm (4.3.4) playable races → Alliance / Horde for UI and spell-range hints.
/// Does not replace `FactionTemplate.dbc` reaction (PvP flags, sanctuary).
enum class PlayableFactionSide : uint8_t {
  Unknown = 0,
  Alliance = 1,
  Horde = 2,
};

inline PlayableFactionSide FactionSideFromPlayableRace(uint8 race) {
  switch (race) {
  case 1:  // Human
  case 3:  // Dwarf
  case 4:  // Night Elf
  case 7:  // Gnome
  case 11: // Draenei
  case 22: // Worgen
    return PlayableFactionSide::Alliance;
  case 2:  // Orc
  case 5:  // Undead
  case 6:  // Tauren
  case 8:  // Troll
  case 9:  // Goblin
  case 10: // Blood Elf
    return PlayableFactionSide::Horde;
  default:
    return PlayableFactionSide::Unknown;
  }
}

/// Spell-range band hints when both units are players. Unknown races leave `outSameTeam`
/// unset and return false.
inline bool TrySpellRangeFriendlyTeamHint(uint8 casterRace, uint8 targetRace,
                                          bool *outSameTeam) {
  if (!outSameTeam)
    return false;

  PlayableFactionSide const a = FactionSideFromPlayableRace(casterRace);
  PlayableFactionSide const b = FactionSideFromPlayableRace(targetRace);
  if (a == PlayableFactionSide::Unknown || b == PlayableFactionSide::Unknown)
    return false;
  *outSameTeam = (a == b);
  return true;
}

} // namespace Firelands
