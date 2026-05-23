#include "DbCreatureSpawnBootstrap.h"
#include <application/world/PhaseGroupCatalog.h>
#include <shared/game/PhaseShift.h>
#include <application/logic/CreatureSpawnLogic.h>
#include <application/services/WorldService.h>
#include <domain/repositories/ICreatureClassLevelStatsRepository.h>
#include <domain/repositories/ICreatureSpawnRepository.h>
#include <domain/world/Creature.h>
#include <shared/dbc/FactionTemplateDbc.h>
#include <shared/game/UnitCombatStats.h>
#include <shared/Logger.h>
#include <shared/game/WowGuid.h>
#include <shared/network/MovementInfo.h>
#include <random>
#include <vector>

namespace Firelands {

std::size_t LoadDatabaseCreatureSpawns(ICreatureSpawnRepository const &spawnRepo,
                                       ICreatureClassLevelStatsRepository const &statsRepo,
                                       PhaseGroupCatalog const &phaseGroups,
                                       FactionTemplateDbc const *factionTemplateDbc) {
  auto const resolveGroup = [&phaseGroups](uint32 groupId) {
    return phaseGroups.Resolve(groupId);
  };
  std::vector<CreatureSpawnRow> const rows = spawnRepo.LoadAllSpawns();
  if (rows.empty()) {
    LOG_INFO("Database creature spawns: none (or missing creature/creature_template)");
    return 0;
  }

  std::mt19937 rng(std::random_device{}());
  std::size_t count = 0;
  std::size_t zeroDbFactionSpawns = 0;
  for (CreatureSpawnRow const &row : rows) {
    uint8 const unitClass = NormalizeCreatureUnitClass(row.unitClass);
    uint8 const level =
        PickCreatureLevelInclusive(row.minLevel, row.maxLevel, rng);
    uint32 const maxHp = statsRepo.BaseHealthFor(level, unitClass);
    uint32 const display = ResolveCreatureDisplayId(
        row.modelId, row.templateModelId1, row.templateModelId2,
        row.templateModelId3, row.templateModelId4);

    MovementInfo mi{};
    mi.x = row.x;
    mi.y = row.y;
    mi.z = row.z;
    mi.orientation = row.orientation;

    uint32 const spawnLow =
        static_cast<uint32>(row.guid & 0xFFFFFFFFu);
    uint64 const objectGuid = MakeCreatureObjectGuid(row.entry, spawnLow);
    uint32_t faction = row.factionTemplate;
    if (faction == 0)
      ++zeroDbFactionSpawns;
    if (factionTemplateDbc != nullptr && factionTemplateDbc->IsLoaded() && faction != 0 &&
        !factionTemplateDbc->HasEntry(faction)) {
      LOG_WARN(
          "creature guid={} entry={} has faction {} not present in FactionTemplate.dbc; "
          "using server fallback",
          row.guid, row.entry, faction);
      faction = 0;
    }
    auto spawned = std::make_shared<Creature>(
        objectGuid, row.entry, display, maxHp, level, faction, row.npcFlags,
        row.experienceModifier);
    spawned->SetPosition(mi);
    spawned->SetCombatStats(BuildCreatureCombatStats(level, unitClass));
    PhaseShift spawnPhase;
    InitDbCreaturePhaseShift(spawnPhase, row.phaseUseFlags, row.phaseId, row.phaseGroup,
                             resolveGroup);
    spawned->SetPhaseShift(std::move(spawnPhase));
    WorldService::Instance().AddCreatureToMap(row.mapId, std::move(spawned));
    ++count;
  }

  if (zeroDbFactionSpawns != 0) {
    LOG_WARN(
        "{} creature spawn(s) use creature_template.faction=0 (common on minimal seeds); "
        "server applies fallback FactionTemplate id {} until you set real faction values.",
        zeroDbFactionSpawns, Creature::kDefaultFactionTemplate);
  }
  LOG_INFO("Database creature spawns: loaded {} NPC(s)", count);
  return count;
}

} // namespace Firelands
