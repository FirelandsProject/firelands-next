#pragma once

#include <shared/Common.h>

namespace Firelands {

/// Cached `creature_classlevelstats.basehealth` keyed by level + creature unit class.
class ICreatureClassLevelStatsRepository {
public:
  virtual ~ICreatureClassLevelStatsRepository() = default;

  /// Level should be 1–255; `unitClass` 0 is normalized to warrior (1) by callers.
  virtual uint32 BaseHealthFor(uint8 level, uint8 unitClass) const = 0;
};

} // namespace Firelands
