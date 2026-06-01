#include <infrastructure/network/sessions/worldsession/WorldSessionSpellEffects.h>

#include <application/combat/MapCombatDamage.h>
#include <application/spell/PassiveSpellAuras.h>
#include <application/spell/PlayerAuraRegenEffects.h>
#include <application/spell/PlayerAuraStatEffects.h>
#include <unordered_set>
#include <application/spell/SpellHitEffects.h>
#include <application/spell/SpellImpactEffects.h>
#include <application/world/WorldRuntimeAccess.h>
#include <shared/game/SpellEffectMagnitude.h>
#include <domain/models/SpellDefinition.h>
#include <domain/world/Aura.h>
#include <domain/world/Creature.h>
#include <domain/world/Map.h>
#include <domain/world/Player.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/Logger.h>
#include <shared/game/SpellAttributes.h>
#include <shared/game/SpellAuraTypes.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <shared/dbc/SpellVisualDbc.h>
#include <shared/network/AuraUpdateWire.h>
#include <shared/network/PlaySpellVisualKitWire.h>
#include <shared/network/SpellPeriodicAuraLogWire.h>
#include <shared/network/WorldPacket.h>
#include <shared/network/packets/server/CombatPackets.h>

#include <functional>
#include <optional>
#include <vector>

namespace Firelands {

namespace {

namespace ws_obj = WorldSessionObjectUpdate;

bool SpellDefinitionHasPhaseAura(SpellDefinition const *def) {
  if (!def)
    return false;
  for (SpellAuraEffectRow const &row : def->auraEffects) {
    if (row.auraType == kSpellAuraPhase || row.auraType == kSpellAuraPhaseGroup ||
        row.auraType == kSpellAuraPhaseAlwaysVisible) {
      return true;
    }
  }
  return def->hasAuraEffect &&
         (def->auraEffectType == kSpellAuraPhase ||
          def->auraEffectType == kSpellAuraPhaseGroup ||
          def->auraEffectType == kSpellAuraPhaseAlwaysVisible);
}

void MaybeRefreshPlayerPhaseAfterAuraChange(std::shared_ptr<Player> const &player,
                                            SpellDefinition const *def) {
  if (!SpellDefinitionHasPhaseAura(def))
    return;
  if (auto ws = std::dynamic_pointer_cast<WorldSession>(player->GetNotifier()))
    ws->RefreshPlayerPhaseVisibilityFromAuras();
}

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

bool ApplyAuraFromOutcome(std::shared_ptr<Map> const &map,
                          SpellCastOutcome const &outcome,
                          std::chrono::steady_clock::time_point now) {
  if (!outcome.hasAuraApply || outcome.auraTargetGuid == 0)
    return false;

  SpellDefinition const *defPtr = nullptr;
  std::optional<SpellDefinition> def;
  if (auto defs = WorldRuntime().GetSpellDefinitions()) {
    def = defs->GetDefinition(outcome.auraSpellId);
    if (def)
      defPtr = &*def;
}
  auto const tables = WorldRuntime().GetSpellCastTables();
  uint32 const durationMs = SpellHitEffects::ResolveAuraDurationMs(
      outcome.auraSpellId, outcome.auraCasterLevel, outcome.auraDurationMs, defPtr,
      tables.get());
  bool const infiniteAura =
      durationMs == 0u && defPtr && defPtr->isPassiveSpell();
  if (durationMs == 0u && !infiniteAura) {
    LOG_WARN("Aura spell {}: no duration from SpellDuration.dbc; not applying (client "
             "timer would desync)",
             outcome.auraSpellId);
    return false;
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
    MaybeRefreshPlayerPhaseAfterAuraChange(target, defPtr);
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
    return false;
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
    return true;
}

} // namespace

void BroadcastUnitHealthAfterDelta(uint32 mapId, std::shared_ptr<Map> const &map,
                                 uint64 unitGuid, uint32 health, uint32 maxHealth) {
  BroadcastUnitHealthAfterDelta(mapId, map, unitGuid, health, maxHealth, nullptr);
}

void BroadcastUnitHealthAfterDelta(uint32 mapId, std::shared_ptr<Map> const &map,
                                 uint64 unitGuid, uint32 health, uint32 maxHealth,
                                 WorldSession *observer) {
  if (!map || unitGuid == 0)
    return;
  WorldPacket hpUpdate;
  ws_obj::BuildUnitHealthValuesUpdate(static_cast<uint16>(mapId), unitGuid, health,
                                      maxHealth, hpUpdate);
  if (observer)
    observer->SendPacket(hpUpdate);
  map->BroadcastPacketToNearby(unitGuid, hpUpdate, true);
}

void BroadcastUnitFlagsOnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                             uint64 unitGuid, uint32 unitFieldFlags) {
  if (!map || unitGuid == 0)
    return;
  WorldPacket pkt;
  ws_obj::BuildUnitFlagsValuesUpdate(static_cast<uint16>(mapId), unitGuid, unitFieldFlags,
                                     pkt);
  map->BroadcastPacketToNearby(unitGuid, pkt, true);
}

void BroadcastUnitDynamicFlagsOnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                                    uint64 unitGuid, uint32 dynamicFlags) {
  if (!map || unitGuid == 0)
    return;
  WorldPacket pkt;
  ws_obj::BuildUnitDynamicFlagsValuesUpdate(static_cast<uint16>(mapId), unitGuid,
                                           dynamicFlags, pkt);
  map->BroadcastPacketToNearby(unitGuid, pkt, true);
}

void BroadcastUnitTargetOnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                              uint64 unitGuid, uint64 targetGuid) {
  if (!map || unitGuid == 0)
    return;
  WorldPacket pkt;
  ws_obj::BuildUnitTargetValuesUpdate(static_cast<uint16>(mapId), unitGuid, targetGuid,
                                      pkt);
  map->BroadcastPacketToNearby(unitGuid, pkt, true);
}

void BroadcastPlayerAuraStatBonusOnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                                     uint64 unitGuid, uint8 casterLevel) {
  if (!map || unitGuid == 0)
    return;
  auto const defs = WorldRuntime().GetSpellDefinitions();
  if (!defs)
    return;
  auto target = map->TryGetPlayer(unitGuid);
  if (!target)
    return;

  std::unordered_set<uint32_t> activeSpellIds;
  for (Aura const &aura : target->GetActiveAuras())
    activeSpellIds.insert(aura.GetSpellId());

  PlayerAuraStatBonus bonus = ComputePlayerAuraStatBonus(
      target->GetActiveAuras(), defs.get(), casterLevel, &target->GetPrimaryStats());
  MergePermanentPassiveSpellBonuses(target->GetKnownPermanentPassiveSpellIds(),
                                    activeSpellIds, defs.get(), casterLevel,
                                    &target->GetPrimaryStats(), bonus);

  UnitCombatStats stats = target->GetBaselineCombatStats();
  ApplyPlayerAuraStatBonusToCombatStats(stats, bonus);
  target->SetCombatStats(stats);
  target->ApplyPassiveHealthPctBonus(bonus.healthPctBonus);

  ResourceRegenModifiers regenMods = ComputePlayerResourceRegenModifiers(
      target->GetActiveAuras(), defs.get(), casterLevel);
  MergePermanentPassiveRegenModifiers(target->GetKnownPermanentPassiveSpellIds(),
                                      activeSpellIds, defs.get(), casterLevel,
                                      regenMods);
  target->SetResourceRegenModifiers(regenMods);
  target->SetCastHasteMultiplier(bonus.castHasteMultiplier);

  WorldPacket pkt;
  ws_obj::BuildPlayerAuraStatValuesUpdate(static_cast<uint16>(mapId), unitGuid, bonus, pkt,
                                          &target->GetBaselineCombatStats(),
                                          target->GetBaselineDodgePct());
  if (bonus.healthPctBonus > 0.f) {
    WorldPacket hpPkt;
    ws_obj::BuildPlayerHealthValuesUpdate(static_cast<uint16>(mapId), unitGuid,
                                          target->GetLiveHealth(), target->GetLiveMaxHealth(),
                                          hpPkt);
    if (auto notifier = target->GetNotifier())
      notifier->SendPacket(hpPkt);
    map->BroadcastPacketToNearby(unitGuid, hpPkt, true);
  }
  // Always deliver to the target session first (login/cast use the same pattern as GM
  // spawns — grid `BroadcastPacketToNearby` can miss the initiating player).
  if (auto notifier = target->GetNotifier())
    notifier->SendPacket(pkt);
  map->BroadcastPacketToNearby(unitGuid, pkt, false);
}

void ApplySpellCastAuraOnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                          SpellCastOutcome const &outcome,
                          std::chrono::steady_clock::time_point now) {
  if (!map)
    return;
  if (!ApplyAuraFromOutcome(map, outcome, now))
    return;
  if (outcome.auraTargetGuid != 0)
    BroadcastPlayerAuraStatBonusOnMap(mapId, map, outcome.auraTargetGuid,
      outcome.auraCasterLevel);
}

void SendActiveAurasOnMap(std::shared_ptr<Map> const &map, uint64 unitGuid,
                          std::chrono::steady_clock::time_point now) {
  SendUnitAurasUpdateAll(map, unitGuid, now);
}

void ApplyPassiveAurasForKnownSpellsOnMap(
    uint32 mapId, std::shared_ptr<Map> const &map, uint64_t unitGuid,
    uint8_t casterLevel, std::vector<uint32_t> const &knownSpellIds,
                          std::chrono::steady_clock::time_point now) {
  if (!map || unitGuid == 0u)
    return;

  auto const defs = WorldRuntime().GetSpellDefinitions();
  auto const tables = WorldRuntime().GetSpellCastTables();
  if (!defs)
    return;

  std::vector<SpellCastOutcome> const outcomes = BuildPassiveAuraOutcomes(
      unitGuid, casterLevel, knownSpellIds, defs.get(), tables.get(), now);

  for (SpellCastOutcome const &outcome : outcomes) {
    if (auto player = map->TryGetPlayer(unitGuid)) {
      if (player->HasAura(outcome.auraSpellId))
        continue;
}
    ApplyAuraFromOutcome(map, outcome, now);
}
  if (!outcomes.empty())
    BroadcastPlayerAuraStatBonusOnMap(mapId, map, unitGuid, casterLevel);
}

void SendAuraRemovalOnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                          uint64 unitGuid, AuraRemoval const &removal) {
  if (!map || unitGuid == 0)
    return;

  auto const now = std::chrono::steady_clock::now();

  LOG_DEBUG("Aura remove: unit={:#x} slot={} spell={}", unitGuid, removal.visualSlot,
            removal.spellId);

    BroadcastAuraPacket(map, unitGuid, [&](WorldPacket &pkt) {
    AuraUpdateWire::BuildAuraRemove(pkt, unitGuid, removal.visualSlot);
    });

  SendUnitAurasUpdateAll(map, unitGuid, now);

  if (auto player = map->TryGetPlayer(unitGuid)) {
    uint8 const level =
        removal.wire.casterLevel > 0 ? removal.wire.casterLevel : 1u;
    BroadcastPlayerAuraStatBonusOnMap(mapId, map, unitGuid, level);
    if (auto defs = WorldRuntime().GetSpellDefinitions()) {
      std::optional<SpellDefinition> def = defs->GetDefinition(removal.spellId);
      if (def)
        MaybeRefreshPlayerPhaseAfterAuraChange(player, &*def);
    }
  }
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

bool RemovePlayerAuraOnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                           uint64 unitGuid, uint32 spellId, uint8 casterLevel) {
  if (!map || unitGuid == 0 || spellId == 0)
    return false;

  if (auto target = map->TryGetPlayer(unitGuid)) {
    auto const removal = target->TryRemoveAura(spellId);
    if (!removal)
    return false;
    SendAuraRemovalOnMap(mapId, map, unitGuid, *removal);
    return true;
}

  if (auto creature = map->TryGetCreature(unitGuid)) {
    auto const removal = creature->TryRemoveAura(spellId);
    if (!removal)
    return false;
    SendAuraRemovalOnMap(mapId, map, unitGuid, *removal);
    return true;
}

    return false;
}

bool RemoveAuraOnMapBySpellId(uint32 mapId, std::shared_ptr<Map> const &map,
                              uint32 spellId, uint64 casterGuid) {
  if (!map || spellId == 0)
    return false;

  auto tryRemove = [&](uint64 guid, auto &unit) -> bool {
    auto const removal = unit.TryRemoveAura(spellId, casterGuid);
    if (!removal)
    return false;
    SendAuraRemovalOnMap(mapId, map, guid, *removal);
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

void BroadcastSpellImpactVisualOnMap(std::shared_ptr<Map> const &map,
                                     uint64 nearbyAnchorGuid, uint32 spellId,
                                     uint64 hitTargetGuid) {
  if (!map || hitTargetGuid == 0 || spellId == 0)
    return;

  auto const defs = WorldRuntime().GetSpellDefinitions();
  auto const visualDbc = WorldRuntime().GetSpellVisualDbc();
  if (!defs || !visualDbc || !visualDbc->IsLoaded())
    return;

  auto def = defs->GetDefinition(spellId);
  if (!def)
    return;

  uint32 const kitId = SpellImpactEffects::ResolveImpactKitForSpell(*def, *visualDbc);
  if (kitId == 0u)
    return;

  WorldPacket impactPkt;
  PlaySpellVisualKitWire::BuildPlaySpellVisualKit(impactPkt, hitTargetGuid,
                                                  static_cast<int32>(kitId));
  map->BroadcastPacketToNearby(nearbyAnchorGuid != 0 ? nearbyAnchorGuid : hitTargetGuid,
                               impactPkt, true);
}

bool ApplyPlayerPower1DeltaOnMap(std::shared_ptr<Map> const &map, uint64 casterGuid,
                                                                  int32 power1Delta) {
    if (!map || casterGuid == 0 || power1Delta == 0)
    return false;
    auto casterPl = map->TryGetPlayer(casterGuid);
    if (!casterPl)
    return false;
    casterPl->ApplyPower1Delta(power1Delta);
    return true;
}

void BroadcastPlayerPower1OnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                                uint64 playerGuid, uint8 primaryPowerType) {
  if (!map || playerGuid == 0)
    return;
  auto pl = map->TryGetPlayer(playerGuid);
  if (!pl)
    return;

  int32 const power = static_cast<int32_t>(pl->GetLivePower1());
  WorldPacket powerPkt =
      combat_wire::BuildPowerUpdate(playerGuid, primaryPowerType, power);

  if (auto notifier = pl->GetNotifier()) {
    notifier->SendPacket(powerPkt);
  }
  map->BroadcastPacketToNearby(playerGuid, powerPkt, true);
}

bool ApplyPlayerSpellPowerCostOnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                                                                        uint64 casterGuid, int32 power1Delta) {
    return ApplyPlayerPower1DeltaOnMap(map, casterGuid, power1Delta);
}

std::optional<CreatureKillByPlayerHint> ApplySpellCastOutcomeOnMap(
    uint32 mapId, std::shared_ptr<Map> const &map, uint64 casterGuid, uint32 spellId,
    SpellCastOutcome const &outcome, std::chrono::steady_clock::time_point now) {
  (void)now;
  (void)spellId;
  if (!map)
    return std::nullopt;

  std::optional<CreatureKillByPlayerHint> killHint;

  if (outcome.hasDirectHealthEffect && outcome.directHealthDelta != 0) {
    int32 const healthDelta = MitigateHealthDeltaOnMap(
        map, outcome.directHealthDelta, outcome.directHealthSchoolMask, casterGuid,
        outcome.directHealthTargetGuid);
    if (auto target = map->TryGetPlayer(outcome.directHealthTargetGuid)) {
      target->ApplyHealthDelta(healthDelta);
      if (healthDelta < 0)
        target->MarkInCombat(now);
      uint32_t const healthAfter = target->GetLiveHealth();
      if (healthDelta < 0 && spellId != 0) {
        uint32_t const damage =
            static_cast<uint32_t>(-healthDelta);
        WorldPacket dmgLog = combat_wire::BuildSpellNonMeleeDamageLog(
            outcome.directHealthTargetGuid, casterGuid, spellId, damage, healthAfter);
        map->BroadcastPacketToNearby(casterGuid, dmgLog, true);
}
      BroadcastUnitHealthAfterDelta(mapId, map, outcome.directHealthTargetGuid,
                                    healthAfter, target->GetLiveMaxHealth());
    } else if (auto cr = map->TryGetCreature(outcome.directHealthTargetGuid)) {
      if (cr->IsEvading())
        return killHint;
      uint32_t const hpBefore = cr->GetLiveHealth();
      cr->ApplyHealthDelta(healthDelta);
      uint32_t const healthAfter = cr->GetLiveHealth();
      if (healthDelta < 0 && spellId != 0) {
        uint32_t const damage =
            static_cast<uint32_t>(-healthDelta);
        WorldPacket dmgLog = combat_wire::BuildSpellNonMeleeDamageLog(
            outcome.directHealthTargetGuid, casterGuid, spellId, damage, healthAfter);
        map->BroadcastPacketToNearby(casterGuid, dmgLog, true);
}
      BroadcastUnitHealthAfterDelta(mapId, map, outcome.directHealthTargetGuid,
                                    healthAfter, cr->GetLiveMaxHealth());
      if (hpBefore > 0 && healthAfter == 0) {
        if (auto killer = map->TryGetPlayer(casterGuid)) {
          if (auto notifier = killer->GetNotifier())
            notifier->OnCreatureKilledByPlayer(outcome.directHealthTargetGuid,
                                               hpBefore);
}
        killHint = CreatureKillByPlayerHint{outcome.directHealthTargetGuid, hpBefore};
}
}
}

  return killHint;
}

} // namespace Firelands
