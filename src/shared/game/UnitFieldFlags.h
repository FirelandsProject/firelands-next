#pragma once

#include <cstdint>

namespace Firelands {

/// `UNIT_FIELD_FLAGS` bits from `creature_template.unit_flags` (client unit update field).
inline constexpr uint32_t kUnitFieldFlagNotSelectable = 0x02000000u;

} // namespace Firelands
