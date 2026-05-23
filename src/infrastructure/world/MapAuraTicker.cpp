#include <infrastructure/world/MapAuraTicker.h>

#include <application/combat/MapCombatDamage.h>
#include <application/world/WorldRuntimeAccess.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <domain/world/Creature.h>
#include <domain/world/Map.h>
#include <domain/world/Player.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionSpellEffects.h>

namespace Firelands {

namespace {

uint32 SchoolMaskForSpell(uint32 spellId) {
  auto const defs = WorldRuntime().GetSpellDefinitions();
  if (!defs)
    return 0u;
  std::optional<SpellDefinition> def = defs->GetDefinition(spellId);
  return def ? def->schoolMask : 0u;
}

template <typename UnitPtr>
void TickUnitAuras(uint32 mapId, std::shared_ptr<Map> const &map, uint64 guid,
                   UnitPtr const &unit, std::chrono::steady_clock::time_point now) {
  UnitAuraTickResult const tick = unit->TickAuras(now);

  for (AuraRemoval const &removal : tick.removals)
    SendAuraRemovalOnMap(mapId, map, guid, removal);

  for (AuraPeriodicTick const &periodic : tick.periodicTicks) {
    if (periodic.healthDelta == 0)
      continue;
    uint32 const schoolMask = SchoolMaskForSpell(periodic.spellId);
    int32 const healthDelta = MitigateHealthDeltaOnMap(
        map, periodic.healthDelta, schoolMask, periodic.casterGuid, guid, true);
    uint32_t const hpBefore = unit->GetLiveHealth();
    unit->ApplyHealthDelta(healthDelta);
    SendPeriodicHealTickOnMap(mapId, map, guid, periodic);
    if (hpBefore > 0 && unit->GetLiveHealth() == 0 && periodic.casterGuid != 0) {
      if (auto killer = map->TryGetPlayer(periodic.casterGuid)) {
        if (auto notifier = killer->GetNotifier())
          notifier->OnCreatureKilledByPlayer(guid, hpBefore);
      }
    }
  }
}

void TickMap(uint32 mapId, std::shared_ptr<Map> const &map,
             std::chrono::steady_clock::time_point now) {
  map->ForEachPlayer([&](std::shared_ptr<Player> const &player) {
    TickUnitAuras(mapId, map, player->GetGuid(), player, now);
  });
  map->ForEachCreature([&](std::shared_ptr<Creature> const &creature) {
    TickUnitAuras(mapId, map, creature->GetGuid(), creature, now);
  });
}

} // namespace

void TickMapAuras(std::chrono::steady_clock::time_point now) {
  WorldRuntime().ForEachMap(
      [&](uint32 mapId, std::shared_ptr<Map> const &map) {
        TickMap(mapId, map, now);
      });
}

} // namespace Firelands
