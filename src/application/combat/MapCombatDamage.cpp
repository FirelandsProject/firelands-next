#include <application/combat/MapCombatDamage.h>

namespace Firelands {

namespace {

UnitCombatStats const *TryPlayerCombatStats(std::shared_ptr<Player> const &player) {
  return player ? &player->GetCombatStats() : nullptr;
}

UnitCombatStats const *TryCreatureCombatStats(std::shared_ptr<Creature> const &creature) {
  return creature ? &creature->GetCombatStats() : nullptr;
}

uint8 ResolveCasterLevel(UnitCombatStats const *casterStats,
                         std::shared_ptr<Player> const &casterPlayer,
                         std::shared_ptr<Creature> const &casterCreature) {
  if (casterStats)
    return casterStats->level;
  if (casterPlayer)
    return casterPlayer->GetCombatStats().level;
  if (casterCreature)
    return casterCreature->GetCombatStats().level;
  return 1;
}

} // namespace

int32 MitigateHealthDeltaOnMap(std::shared_ptr<Map> const &map, int32 rawSignedDelta,
                               uint32 schoolMask, uint64 casterGuid,
                               uint64 victimGuid, bool periodicTick) {
  if (rawSignedDelta >= 0 || !map)
    return rawSignedDelta;

  std::shared_ptr<Player> casterPlayer;
  std::shared_ptr<Creature> casterCreature;
  if (casterGuid != 0) {
    casterPlayer = map->TryGetPlayer(casterGuid);
    if (!casterPlayer)
      casterCreature = map->TryGetCreature(casterGuid);
  }

  std::shared_ptr<Player> victimPlayer;
  std::shared_ptr<Creature> victimCreature;
  victimPlayer = map->TryGetPlayer(victimGuid);
  if (!victimPlayer)
    victimCreature = map->TryGetCreature(victimGuid);

  UnitCombatStats const *casterStats =
      TryPlayerCombatStats(casterPlayer) ? TryPlayerCombatStats(casterPlayer)
                                         : TryCreatureCombatStats(casterCreature);
  UnitCombatStats const *victimStats =
      TryPlayerCombatStats(victimPlayer) ? TryPlayerCombatStats(victimPlayer)
                                         : TryCreatureCombatStats(victimCreature);

  uint8 const casterLevel =
      ResolveCasterLevel(casterStats, casterPlayer, casterCreature);

  return ResolveMitigatedHealthDelta(rawSignedDelta, schoolMask, casterStats,
                                     casterLevel, victimStats, periodicTick);
}

} // namespace Firelands
