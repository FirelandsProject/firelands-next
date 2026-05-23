#pragma once

#include <domain/world/Creature.h>
#include <domain/world/Map.h>
#include <domain/world/Player.h>
#include <memory>
#include <shared/game/CombatDamage.h>
#include <shared/game/UnitCombatStats.h>

namespace Firelands {

/// Resolves caster/victim combat stats on a map and applies reference mitigation.
int32 MitigateHealthDeltaOnMap(std::shared_ptr<Map> const &map, int32 rawSignedDelta,
                               uint32 schoolMask, uint64 casterGuid,
                               uint64 victimGuid, bool periodicTick = false);

} // namespace Firelands
