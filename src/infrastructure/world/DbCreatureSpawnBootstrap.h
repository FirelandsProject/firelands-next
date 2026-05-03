#pragma once

#include <cstddef>

namespace Firelands {

class ICreatureSpawnRepository;
class ICreatureClassLevelStatsRepository;

/// Instantiates `Creature` objects from `creature` + applies `creature_classlevelstats`
/// for max health. Notifies players already online via map-wide broadcast (startup is
/// typically empty); logging-in players receive nearby creatures via `WorldSession`.
std::size_t LoadDatabaseCreatureSpawns(ICreatureSpawnRepository const &spawnRepo,
                                       ICreatureClassLevelStatsRepository const &statsRepo);

} // namespace Firelands
