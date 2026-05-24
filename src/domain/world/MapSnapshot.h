#pragma once

#include <shared/Common.h>

namespace Firelands {

/// Immutable point-in-time map observability data.
struct MapSnapshot {
  uint32 mapId = 0;
  int playerCount = 0;
  int creatureCount = 0;
  int loadedGridCells = 0;
  double avgTickTimeMs = 0.0;
  double lastTickTimeMs = 0.0;
  bool isEmpty = true;
};

} // namespace Firelands
