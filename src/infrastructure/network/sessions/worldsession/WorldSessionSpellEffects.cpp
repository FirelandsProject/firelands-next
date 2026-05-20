#include <infrastructure/network/sessions/worldsession/WorldSessionSpellEffects.h>

#include <application/spell/PassiveSpellAuras.h>
#include <application/spell/SpellHitEffects.h>
#include <application/services/WorldService.h>
#include <shared/game/SpellEffectMagnitude.h>
#include <domain/world/Aura.h>
#include <domain/world/Creature.h>
#include <domain/world/Map.h>
#include <domain/world/Player.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/Logger.h>
#include <shared/game/SpellAttributes.h>
#include <shared/game/SpellAuraTypes.h>
#include <shared/network/AuraUpdateWire.h>
#include <shared/network/SpellPeriodicAuraLogWire.h>
#include <shared/network/WorldPacket.h>

#include <functional>
#include <optional>
#include <vector>

namespace Firelands {

namespace {

namespace ws_obj = WorldSessionObjectUpdate;

void BroadcastAuraPacket(std::shared_ptr<Map> const &map, uint64 unitGuid,
                         std::function<void(WorldPacket &)> const &buildPacket) {
  if (!map || unitGuid == 0)
    return;
  WorldPacket pkt;
  buildPacket(pkt);
  map->BroadcastPacketToNearby(unitGuid, pkt, true);
}

AuraUpdateWire::AuraApplyParams ParamsFromAura(Aura const &aura,
                                             std::chrono::steady_clock::time_point now) {
  AuraClientWireMeta const &wire = aura.GetClientWireMeta();
  AuraUpdateWire::AuraApplyParams params{};
  params.visualSlot = aura.GetVisualSlot();
  params.spellId = aura.GetSpellId();
  params.effectIndex = wire.effectIndex;
  params.activeEffectMask = wire.activeEffectMask != 0u
                                ? wire.activeEffectMask
                                : static_cast<uint8>(1u << wire.effectIndex);
  params.casterLevel = wire.casterLevel;
  params.casterGuid = wire.casterGuid;
  params.durationMs = wire.maxDurationMs;
  params.remainingMs = static_cast<uint32>(aura.GetRemainingMs(now).count());
  params.explicitRemaining = true;
  params.isNegative = wire.isNegative;
  params.effectAmount = wire.effectAmount;
  params.sendEffectAmount = wire.sendEffectAmount;
  return params;
}

template <typename Unit>
std::vector<AuraUpdateWire::AuraApplyParams>
CollectActiveAuraParams(Unit const &unit, std::chrono::steady_clock::time_point now) {
  std::vector<AuraUpdateWire::AuraApplyParams> params;
  for (Aura const &aura : unit.GetActiveAuras())
    params.push_back(ParamsFromAura(aura, now));
  return params;
}

void SendUnitAurasUpdateAll(std::shared_ptr<Map> const &map, uint64 unitGuid,
                            std::chrono::steady_clock::time_point now) {
  if (!map || unitGuid == 0)
    return;

  if (auto target = map->TryGetPlayer(unitGuid)) {
    auto const params = CollectActiveAuraParams(*target, now);
    BroadcastAuraPacket(map, unitGuid, [&](WorldPacket &pkt) {
      AuraUpdateWire::BuildAuraUpdateAll(pkt, unitGuid, params);
    });
    return;
  }

  if (auto creature = map->TryGetCreature(unitGuid)) {
    auto const params = CollectActiveAuraParams(*creature, now);
    BroadcastAuraPacket(map, unitGuid, [&](WorldPacket &pkt) {
      AuraUpdateWire::BuildAuraUpdateAll(pkt, unitGuid, params);
    });
  }
}

void BroadcastUnitHealthAfterDelta(uint32 mapId, std::shared_ptr<Map> const &map,
                                 uint64 unitGuid, uint32 health, uint32 maxHealth) {
  WorldPacket hpUpdate;
  ws_obj::BuildPlayerHealthValuesUpdate(static_cast<uint16>(mapId), unitGuid, health,
                                        maxHealth, hpUpdate);
  map->BroadcastPacketToNearby(unitGuid, hpUpdate, true);
}

int32 ResolveAuraWireEffectAmount(SpellCastOutcome const &outcome,
                                  SpellDefinition const *def) {
  if (outcome.auraPeriodicHealthDeltaPerTick != 0)
    return outcome.auraPeriodicHealthDeltaPerTick;
  if (!def || !def->sendsAuraEffectAmountOnWire())
    return 0;
  return SpellEffectMagnitude::NeutralMagnitudeAtLevel(
      def->auraBasePoints, def->auraDieSides, def->auraRealPointsPerLevel,
      outcome.auraCasterLevel);
}

void ApplyAuraFromOutcome(std::shared_ptr<Map> const &map,
                          SpellCastOutcome const &outcome,
                          std::chrono::steady_clock::time_point now) {
  if (!outcome.hasAuraApply || outcome.auraTargetGuid == 0)
    return;

  SpellDefinition const *defPtr = nullptr;
  std::optional<SpellDefinition> def;
  if (auto defs = WorldService::Instance().GetSpellDefinitions()) {
    def = defs->GetDefinition(outcome.auraSpellId);
    if (def)
      defPtr = &*def;
  }
  auto const tables = WorldService::Instance().GetSpellCastTables();
  uint32 const durationMs = SpellHitEffects::ResolveAuraDurationMs(
      outcome.auraSpellId, outcome.auraCasterLevel, outcome.auraDurationMs, defPtr,
      tables.get());
  bool const infiniteAura =
      durationMs == 0u && defPtr && defPtr->isPassiveSpell();
  if (durationMs == 0u && !infiniteAura) {
    LOG_WARN("Aura spell {}: no duration from SpellDuration.dbc; not applying (client "
             "timer would desync)",
             outcome.auraSpellId);
    return;
  }

  int32 const wireEffectAmount = ResolveAuraWireEffectAmount(outcome, defPtr);
  auto const expireTime = infiniteAura
                              ? std::chrono::steady_clock::time_point::max()
                              : now + std::chrono::milliseconds(durationMs);
  uint32 const wireDurationMs = infiniteAura ? 0u : durationMs;

  uint8 slot = 0;
  if (auto target = map->TryGetPlayer(outcome.auraTargetGuid)) {
    slot = target->AllocateAuraVisualSlot(outcome.auraSpellId);
    auto const nextTick = now;

    uint8 const effectMask =
        defPtr && defPtr->auraActiveEffectMask != 0u
            ? defPtr->auraActiveEffectMask
            : static_cast<uint8>(1u << outcome.auraEffectIndex);
    bool const sendAmount =
        defPtr && defPtr->sendsAuraEffectAmountOnWire() && wireEffectAmount != 0;

    AuraClientWireMeta wire{};
    wire.effectIndex = outcome.auraEffectIndex;
    wire.activeEffectMask = effectMask;
    wire.casterLevel = outcome.auraCasterLevel;
    wire.casterGuid = outcome.auraCasterGuid;
    wire.maxDurationMs = wireDurationMs;
    wire.isNegative = outcome.auraIsNegative;
    if (sendAmount) {
      wire.effectAmount = wireEffectAmount;
      wire.sendEffectAmount = true;
    }
    Aura aura(outcome.auraSpellId, outcome.auraEffectType, outcome.auraBasePoints,
              outcome.auraDieSides, outcome.auraCasterGuid, expireTime, slot,
              outcome.auraPeriodicPeriodMs, outcome.auraPeriodicHealthDeltaPerTick,
              nextTick, wire);
    target->AddAura(aura);
  } else if (auto creature = map->TryGetCreature(outcome.auraTargetGuid)) {
    slot = creature->AllocateAuraVisualSlot(outcome.auraSpellId);
    auto const nextTick = now;

    uint8 const effectMask =
        defPtr && defPtr->auraActiveEffectMask != 0u
            ? defPtr->auraActiveEffectMask
            : static_cast<uint8>(1u << outcome.auraEffectIndex);
    bool const sendAmount =
        defPtr && defPtr->sendsAuraEffectAmountOnWire() && wireEffectAmount != 0;

    AuraClientWireMeta wire{};
    wire.effectIndex = outcome.auraEffectIndex;
    wire.activeEffectMask = effectMask;
    wire.casterLevel = outcome.auraCasterLevel;
    wire.casterGuid = outcome.auraCasterGuid;
    wire.maxDurationMs = wireDurationMs;
    wire.isNegative = outcome.auraIsNegative;
    if (sendAmount) {
      wire.effectAmount = wireEffectAmount;
      wire.sendEffectAmount = true;
    }
    Aura aura(outcome.auraSpellId, outcome.auraEffectType, outcome.auraBasePoints,
              outcome.auraDieSides, outcome.auraCasterGuid, expireTime, slot,
              outcome.auraPeriodicPeriodMs, outcome.auraPeriodicHealthDeltaPerTick,
              nextTick, wire);
    creature->AddAura(aura);
  } else {
    return;
  }

  uint8 const effectMask =
      defPtr && defPtr->auraActiveEffectMask != 0u
          ? defPtr->auraActiveEffectMask
          : static_cast<uint8>(1u << outcome.auraEffectIndex);
  bool const sendAmount =
      defPtr && defPtr->sendsAuraEffectAmountOnWire() && wireEffectAmount != 0;

  AuraUpdateWire::AuraApplyParams params{};
  params.visualSlot = slot;
  params.spellId = outcome.auraSpellId;
  params.effectIndex = outcome.auraEffectIndex;
  params.activeEffectMask = effectMask;
  params.casterLevel = outcome.auraCasterLevel;
  params.casterGuid = outcome.auraCasterGuid;
  params.durationMs = wireDurationMs;
  params.remainingMs = wireDurationMs;
  params.isNegative = outcome.auraIsNegative;
  if (sendAmount) {
    params.effectAmount = wireEffectAmount;
    params.sendEffectAmount = true;
  }
  BroadcastAuraPacket(map, outcome.auraTargetGuid, [&](WorldPacket &pkt) {
    AuraUpdateWire::BuildAuraApply(pkt, outcome.auraTargetGuid, params);
  });
}

} // namespace

void ApplySpellCastAuraOnMap(std::shared_ptr<Map> const &map,
                             SpellCastOutcome const &outcome,
                             std::chrono::steady_clock::time_point now) {
  if (!map)
    return;
  ApplyAuraFromOutcome(map, outcome, now);
}

void SendActiveAurasOnMap(std::shared_ptr<Map> const &map, uint64 unitGuid,
                          std::chrono::steady_clock::time_point now) {
  SendUnitAurasUpdateAll(map, unitGuid, now);
}

void ApplyPassiveAurasForKnownSpellsOnMap(
    std::shared_ptr<Map> const &map, uint64_t unitGuid, uint8_t casterLevel,
    std::vector<uint32_t> const &knownSpellIds,
    std::chrono::steady_clock::time_point now) {
  if (!map || unitGuid == 0u)
    return;

  auto const defs = WorldService::Instance().GetSpellDefinitions();
  auto const tables = WorldService::Instance().GetSpellCastTables();
  if (!defs)
    return;

  std::vector<SpellCastOutcome> const outcomes = BuildPassiveAuraOutcomes(
      unitGuid, casterLevel, knownSpellIds, defs.get(), tables.get(), now);

  for (SpellCastOutcome const &outcome : outcomes) {
    if (auto player = map->TryGetPlayer(unitGuid)) {
      if (player->HasAura(outcome.auraSpellId))
        continue;
    }
    ApplySpellCastAuraOnMap(map, outcome, now);
  }
}

void SendAuraRemovalOnMap(std::shared_ptr<Map> const &map, uint64 unitGuid,
                          AuraRemoval const &removal) {
  if (!map || unitGuid == 0)
    return;

  auto const now = std::chrono::steady_clock::now();

  LOG_DEBUG("Aura remove: unit={:#x} slot={} spell={}", unitGuid, removal.visualSlot,
            removal.spellId);

  BroadcastAuraPacket(map, unitGuid, [&](WorldPacket &pkt) {
    AuraUpdateWire::BuildAuraRemove(pkt, unitGuid, removal.visualSlot);
  });

  SendUnitAurasUpdateAll(map, unitGuid, now);
}

void SendPeriodicHealTickOnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                               uint64 unitGuid, AuraPeriodicTick const &tick) {
  if (!map || unitGuid == 0 || tick.healthDelta == 0)
    return;

  if (tick.auraEffectType == kSpellAuraPeriodicHeal && tick.casterGuid != 0u) {
    uint32 const healAmount =
        tick.healthDelta > 0 ? static_cast<uint32>(tick.healthDelta) : 0u;
    BroadcastAuraPacket(map, unitGuid, [&](WorldPacket &pkt) {
      SpellPeriodicAuraLogWire::BuildPeriodicHeal(pkt, unitGuid, tick.casterGuid,
                                                  tick.spellId, healAmount);
    });
  }

  if (tick.wire.maxDurationMs > 0u) {
    AuraUpdateWire::AuraApplyParams refresh{};
    refresh.visualSlot = tick.visualSlot;
    refresh.spellId = tick.spellId;
    refresh.effectIndex = tick.wire.effectIndex;
    refresh.activeEffectMask = tick.wire.activeEffectMask != 0u
                                   ? tick.wire.activeEffectMask
                                   : static_cast<uint8>(1u << tick.wire.effectIndex);
    refresh.casterLevel = tick.wire.casterLevel;
    refresh.casterGuid = tick.wire.casterGuid;
    refresh.durationMs = tick.wire.maxDurationMs;
    refresh.remainingMs = tick.remainingMs;
    refresh.explicitRemaining = true;
    refresh.isNegative = tick.wire.isNegative;
    refresh.effectAmount = tick.wire.effectAmount;
    refresh.sendEffectAmount = tick.wire.sendEffectAmount;
    BroadcastAuraPacket(map, unitGuid, [&](WorldPacket &pkt) {
      AuraUpdateWire::BuildAuraApply(pkt, unitGuid, refresh);
    });
  }

  if (auto target = map->TryGetPlayer(unitGuid)) {
    BroadcastUnitHealthAfterDelta(mapId, map, unitGuid, target->GetLiveHealth(),
                                  target->GetLiveMaxHealth());
  } else if (auto creature = map->TryGetCreature(unitGuid)) {
    BroadcastUnitHealthAfterDelta(mapId, map, unitGuid, creature->GetLiveHealth(),
                                  creature->GetLiveMaxHealth());
  }
}

bool RemovePlayerAuraOnMap(std::shared_ptr<Map> const &map, uint64 unitGuid,
                           uint32 spellId) {
  if (!map || unitGuid == 0 || spellId == 0)
    return false;

  if (auto target = map->TryGetPlayer(unitGuid)) {
    auto const removal = target->TryRemoveAura(spellId);
    if (!removal)
      return false;
    SendAuraRemovalOnMap(map, unitGuid, *removal);
    return true;
  }

  if (auto creature = map->TryGetCreature(unitGuid)) {
    auto const removal = creature->TryRemoveAura(spellId);
    if (!removal)
      return false;
    SendAuraRemovalOnMap(map, unitGuid, *removal);
    return true;
  }

  return false;
}

bool RemoveAuraOnMapBySpellId(std::shared_ptr<Map> const &map, uint32 spellId,
                              uint64 casterGuid) {
  if (!map || spellId == 0)
    return false;

  auto tryRemove = [&](uint64 guid, auto &unit) -> bool {
    auto const removal = unit.TryRemoveAura(spellId, casterGuid);
    if (!removal)
      return false;
    SendAuraRemovalOnMap(map, guid, *removal);
    return true;
  };

  bool removed = false;
  map->ForEachPlayer([&](std::shared_ptr<Player> const &player) {
    if (tryRemove(player->GetGuid(), *player))
      removed = true;
  });
  map->ForEachCreature([&](std::shared_ptr<Creature> const &creature) {
    if (tryRemove(creature->GetGuid(), *creature))
      removed = true;
  });
  return removed;
}

void ApplySpellCastOutcomeOnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                                uint64 casterGuid, SpellCastOutcome const &outcome,
                                std::chrono::steady_clock::time_point now) {
  (void)now;
  if (!map)
    return;

  if (outcome.hasDirectHealthEffect && outcome.directHealthDelta != 0) {
    if (auto target = map->TryGetPlayer(outcome.directHealthTargetGuid)) {
      target->ApplyHealthDelta(outcome.directHealthDelta);
      BroadcastUnitHealthAfterDelta(mapId, map, outcome.directHealthTargetGuid,
                                    target->GetLiveHealth(), target->GetLiveMaxHealth());
    } else if (auto cr = map->TryGetCreature(outcome.directHealthTargetGuid)) {
      cr->ApplyHealthDelta(outcome.directHealthDelta);
      BroadcastUnitHealthAfterDelta(mapId, map, outcome.directHealthTargetGuid,
                                    cr->GetLiveHealth(), cr->GetLiveMaxHealth());
    }
  }

  if (outcome.power1Delta != 0 && casterGuid != 0) {
    if (auto casterPl = map->TryGetPlayer(casterGuid)) {
      casterPl->ApplyPower1Delta(outcome.power1Delta);
      WorldPacket pwUpdate;
      ws_obj::BuildPlayerPower1ValuesUpdate(static_cast<uint16>(mapId), casterGuid,
                                            casterPl->GetLivePower1(),
                                            casterPl->GetLiveMaxPower1(), pwUpdate);
      map->BroadcastPacketToNearby(casterGuid, pwUpdate, true);
    }
  }
}

} // namespace Firelands
