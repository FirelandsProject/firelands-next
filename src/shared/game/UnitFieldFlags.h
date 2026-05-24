#pragma once

#include <cstdint>

namespace Firelands {

/// Cataclysm 4.3.4 `UNIT_FIELD_FLAGS` bits (reference `UnitDefines.h`).
inline constexpr uint32_t kUnitFlagCanSwim = 0x00008000u;
inline constexpr uint32_t kUnitFlagInCombat = 0x00080000u;
/// `creature_template.unit_flags` / GM visibility.
inline constexpr uint32_t kUnitFieldFlagNotSelectable = 0x02000000u;

/// `UNIT_DYNAMIC_FLAGS` bits (reference `UnitDefines.h`).
inline constexpr uint32_t kUnitDynflagLootable = 0x00000001u;
inline constexpr uint32_t kUnitDynflagTappedByPlayer = 0x00000008u;

} // namespace Firelands
