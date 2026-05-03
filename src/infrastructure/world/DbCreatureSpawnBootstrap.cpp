#include "DbCreatureSpawnBootstrap.h"
#include <application/logic/CreatureSpawnLogic.h>
#include <application/services/WorldService.h>
#include <domain/repositories/ICreatureClassLevelStatsRepository.h>
#include <domain/repositories/ICreatureSpawnRepository.h>
#include <domain/world/Creature.h>
#include <domain/world/Map.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/Logger.h>
#include <shared/network/MovementInfo.h>
#include <shared/network/UpdateData.h>
#include <random>
#include <vector>

namespace Firelands {

namespace {

constexpr uint32 kFallbackDisplayIfModelMissing = 169u;

void BroadcastCreatureCreateFullMap(uint32 mapId, uint64 creatureGuid,
                                    MovementInfo const &move, uint32 entry,
                                    uint32 displayId, uint32 hp, uint32 maxHp,
                                    uint8 level) {
  auto fields = WorldSessionObjectUpdate::BuildMinimalNpcUnitCreateFields(
      creatureGuid, entry, displayId, hp, maxHp, level, 0u);
  UpdateData update(static_cast<uint16>(mapId));
  update.AddCreateObject(creatureGuid, TYPEID_UNIT, move, fields);
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  WorldService::Instance().GetMap(mapId)->BroadcastPacket(creatureGuid, pkt, false);
}

} // namespace

std::size_t LoadDatabaseCreatureSpawns(ICreatureSpawnRepository const &spawnRepo,
                                       ICreatureClassLevelStatsRepository const &statsRepo) {
  std::vector<CreatureSpawnRow> const rows = spawnRepo.LoadAllSpawns();
  if (rows.empty()) {
    LOG_INFO("Database creature spawns: none (or missing creature/creature_template)");
    return 0;
  }

  std::mt19937 rng(std::random_device{}());
  std::size_t count = 0;
  for (CreatureSpawnRow const &row : rows) {
    uint8 const unitClass = NormalizeCreatureUnitClass(row.unitClass);
    uint8 const level =
        PickCreatureLevelInclusive(row.minLevel, row.maxLevel, rng);
    uint32 const maxHp = statsRepo.BaseHealthFor(level, unitClass);
    uint32 const display =
        row.modelId != 0 ? row.modelId : kFallbackDisplayIfModelMissing;

    MovementInfo mi{};
    mi.x = row.x;
    mi.y = row.y;
    mi.z = row.z;
    mi.orientation = row.orientation;

    auto spawned =
        std::make_shared<Creature>(row.guid, row.entry, display, maxHp, level);
    spawned->SetPosition(mi);
    WorldService::Instance().AddCreatureToMap(row.mapId, std::move(spawned));

    BroadcastCreatureCreateFullMap(row.mapId, row.guid, mi, row.entry, display,
                                   maxHp, maxHp, level);
    ++count;
  }

  LOG_INFO("Database creature spawns: loaded {} NPC(s)", count);
  return count;
}

} // namespace Firelands
