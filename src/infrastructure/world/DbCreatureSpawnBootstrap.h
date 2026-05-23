#pragma once

#include <cstddef>

namespace Firelands {

class ICreatureSpawnRepository;
class ICreatureClassLevelStatsRepository;
class FactionTemplateDbc;
class PhaseGroupCatalog;

/// Instantiates `Creature` objects from `creature` + applies `creature_classlevelstats`
/// for max health. Notifies players already online via map-wide broadcast (startup is
/// typically empty); logging-in players receive nearby creatures via `WorldSession`.
/// \param factionTemplateDbc When loaded, `creature_template.faction` ids are validated;
///        unknown ids fall back to `Creature` default faction (see `Creature::kDefaultFactionTemplate`).
std::size_t LoadDatabaseCreatureSpawns(ICreatureSpawnRepository const &spawnRepo,
                                       ICreatureClassLevelStatsRepository const &statsRepo,
                                       PhaseGroupCatalog const &phaseGroups,
                                       FactionTemplateDbc const *factionTemplateDbc = nullptr);

} // namespace Firelands
