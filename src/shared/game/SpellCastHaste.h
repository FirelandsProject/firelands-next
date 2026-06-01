#pragma once

#include <shared/Common.h>
#include <cmath>
#include <cstdint>

namespace Firelands {

/// Scales base cast time by haste multiplier (1.0 = none, 1.21 = 21% faster).
inline uint32_t ApplyCastHasteMultiplierToCastTimeMs(uint32_t baseMs,
                                                    float castHasteMultiplier) {
  if (baseMs == 0u || castHasteMultiplier <= 1.f)
    return baseMs;
  double const scaled =
      static_cast<double>(baseMs) / static_cast<double>(castHasteMultiplier);
  return static_cast<uint32_t>(std::max(0.0, std::floor(scaled)));
}

} // namespace Firelands
