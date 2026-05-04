#pragma once

#include <shared/Common.h>

namespace Firelands {

/// Client movement state (`MovementFlags` / `MovementFlags2` in MovementFlags.h).
struct MovementInfo {
  uint32 flags = 0;
  uint16 flags2 = 0;
  uint32 time = 0;
  float x = 0.0f, y = 0.0f, z = 0.0f, orientation = 0.0f;
  uint32 fallTime = 0;
};

} // namespace Firelands
