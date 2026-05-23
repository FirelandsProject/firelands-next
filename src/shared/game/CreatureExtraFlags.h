#pragma once

#include <cstdint>

namespace Firelands {

/// `creature_template.flags_extra` — server-side template metadata (not sent as an update field).
inline constexpr uint32_t kCreatureExtraFlagTrigger = 0x00000080u;

} // namespace Firelands
