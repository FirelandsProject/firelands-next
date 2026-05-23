#pragma once

#include <domain/models/CreatureSpawn.h>
#include <vector>

namespace Firelands {

/// Loads static NPC placements from `creature` joined with `creature_template`.
class ICreatureSpawnRepository {
public:
  virtual ~ICreatureSpawnRepository() = default;

  virtual std::vector<CreatureSpawn> LoadAllSpawns() const = 0;
};

} // namespace Firelands
