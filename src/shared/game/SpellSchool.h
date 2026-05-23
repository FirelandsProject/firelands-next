#pragma once

#include <shared/Common.h>
#include <algorithm>
#include <cstdint>

namespace Firelands {

/// Blizzard spell school bitmask (`Spell.dbc` SchoolMask).
constexpr uint32 kSpellSchoolMaskNormal = 1u << 0;
constexpr uint32 kSpellSchoolMaskHoly = 1u << 1;
constexpr uint32 kSpellSchoolMaskFire = 1u << 2;
constexpr uint32 kSpellSchoolMaskNature = 1u << 3;
constexpr uint32 kSpellSchoolMaskFrost = 1u << 4;
constexpr uint32 kSpellSchoolMaskShadow = 1u << 5;
constexpr uint32 kSpellSchoolMaskArcane = 1u << 6;
constexpr uint32 kSpellSchoolMaskMagic =
    kSpellSchoolMaskHoly | kSpellSchoolMaskFire | kSpellSchoolMaskNature |
    kSpellSchoolMaskFrost | kSpellSchoolMaskShadow | kSpellSchoolMaskArcane;

inline uint8 FirstSchoolFromMask(uint32 schoolMask) {
  if (schoolMask == 0u)
    return 0;
  for (uint8 i = 0; i < 7; ++i) {
    if ((schoolMask & (1u << i)) != 0u)
      return i;
  }
  return 0;
}

inline bool SchoolMaskIsPhysical(uint32 schoolMask) {
  return (schoolMask & kSpellSchoolMaskNormal) != 0u ||
         schoolMask == 0u;
}

inline bool SchoolMaskIsMagic(uint32 schoolMask) {
  return (schoolMask & kSpellSchoolMaskMagic) != 0u;
}

} // namespace Firelands
