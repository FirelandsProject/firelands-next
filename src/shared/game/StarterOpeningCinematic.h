#pragma once

#include <shared/Common.h>
#include <cstdint>

namespace Firelands {

/// `SMSG_TRIGGER_CINEMATIC` uses ids from the client's `CinematicSequences.dbc`.
/// Race ids match `ChrRaces.dbc` (build 15595 / parity with post-Cata ChrRaces rows).
/// Death Knight uses `ChrClasses.dbc` camera / cinematic sequence (see wiki).
inline uint32_t OpeningCinematicSequence(uint8 klass, uint8 race) {
  constexpr uint8_t kClassDeathKnight = 6;
  if (klass == kClassDeathKnight)
    return 165u;

  switch (race) {
  case 1:
    return 81u; // Human
  case 2:
    return 21u; // Orc
  case 3:
    return 41u; // Dwarf
  case 4:
    return 61u; // Night Elf
  case 5:
    return 2u; // Undead
  case 6:
    return 141u; // Tauren
  case 7:
    return 101u; // Gnome
  case 8:
    return 121u; // Troll
  case 9:
    return 172u; // Goblin
  case 10:
    return 162u; // Blood Elf
  case 11:
    return 163u; // Draenei
  case 22:
    return 170u; // Worgen
  default:
    return 0u;
  }
}

} // namespace Firelands
