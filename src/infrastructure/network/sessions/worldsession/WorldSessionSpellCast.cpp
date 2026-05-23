#include <application/ports/IMapCollisionQueries.h>
#include <application/services/WorldService.h>
#include <boost/asio/redirect_error.hpp>
#include <infrastructure/network/asio/AsioAwaitables.h>
#include <application/spell/SpellManager.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionSpellEffects.h>
#include <shared/Logger.h>
#include <shared/game/PlayerFactionTeam.h>
#include <shared/game/SpellPowerCost.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <shared/network/SpellCastWire.h>
#include <shared/network/WorldPacket.h>

#include <algorithm>
#include <utility>

namespace Firelands {

void WorldSession::CancelPendingClientSpellCast() {
  (void)_pendingSpellCastTimer.cancel();
  _pendingDeferredCastActive = false;
  _pendingCastId = 0;
  _pendingSpellId = 0;
  StopMeleeAutoAttack(true);
  }

void WorldSession::ScheduleDeferredSpellCastCompletion(SpellCastOutcome const &out) {
  (void)_pendingSpellCastTimer.cancel();

  PendingSpellCastFinish finish{};
  finish.mapId = _mapId;
  finish.casterGuid = _playerGuid;
  finish.castId = out.deferredCastId;
  finish.spellId = out.deferredSpellId;
  finish.targetFlags = out.deferredTargetFlags;
  finish.targetUnitGuid = out.deferredTargetUnitGuid;
  finish.hitGuid = out.deferredHitGuid;
  finish.hasDirectHealthEffect = out.hasDirectHealthEffect;
  finish.directHealthTargetGuid = out.directHealthTargetGuid;
  finish.directHealthDelta = out.directHealthDelta;
  finish.power1Delta = out.power1Delta;
  finish.spellCooldownDurationMs = out.spellCooldownDurationMs;
  finish.spellCategoryCooldownGroup = out.spellCategoryCooldownGroup;
  finish.spellCategoryCooldownDurationMs = out.spellCategoryCooldownDurationMs;
  finish.hasAuraApply = out.hasAuraApply;
  finish.auraTargetGuid = out.auraTargetGuid;
  finish.auraCasterGuid = out.auraCasterGuid;
  finish.auraSpellId = out.auraSpellId;
  finish.auraEffectType = out.auraEffectType;
  finish.auraEffectIndex = out.auraEffectIndex;
  finish.auraBasePoints = out.auraBasePoints;
  finish.auraDieSides = out.auraDieSides;
  finish.auraDurationMs = out.auraDurationMs;
  finish.auraPeriodicPeriodMs = out.auraPeriodicPeriodMs;
  finish.auraPeriodicHealthDeltaPerTick = out.auraPeriodicHealthDeltaPerTick;
  finish.auraIsNegative = out.auraIsNegative;
  finish.auraCasterLevel = out.auraCasterLevel;
  finish.spellGoMissile = out.deferredSpellGoMissile;
  finish.missile = out.deferredMissile;
  finish.spellImpactDelayMs = out.spellImpactDelayMs;

  _pendingDeferredCastActive = true;
  _pendingCastId = out.deferredCastId;
  _pendingSpellId = out.deferredSpellId;

  _pendingSpellCastTimer.expires_after(std::chrono::milliseconds(
      static_cast<int64_t>(std::max<uint32_t>(1u, out.deferredCastTimeMs))));
  auto self = shared_from_this();
  Asio::SpawnDetached(_socket.get_executor(),
                      [self, this, finish]() -> Asio::awaitable<void> {
                        boost::system::error_code ec;
                        co_await _pendingSpellCastTimer.async_wait(
                            boost::asio::redirect_error(Asio::use_awaitable, ec));
                        if (!ec)
                          CompleteDeferredSpellCast(finish);
                      });
  }

void WorldSession::CompleteDeferredSpellCast(PendingSpellCastFinish const &finish) {
  if (_playerGuid != finish.casterGuid || finish.casterGuid == 0 || _mapId != finish.mapId)
    return;

  _pendingDeferredCastActive = false;
  _pendingCastId = 0;
  _pendingSpellId = 0;

  uint32 const castFlagsGo =
      SpellCastWire::BuildSpellGoCastFlags(finish.spellGoMissile);
  auto const nowGo = std::chrono::steady_clock::now();
  uint32 const castTimeGo =
      SpellCastWire::ResolveSpellGoTimestampMs(_position.time);
  WorldPacket spellGo;
  uint64 const hitTargets[1] = {finish.hitGuid};
  SpellCastWire::SpellMissileTrajectoryWire const *missilePtr =
      finish.spellGoMissile ? &finish.missile : nullptr;
  SpellCastWire::BuildSpellGo(spellGo, finish.casterGuid, finish.castId, finish.spellId,
                              castFlagsGo, 0, castTimeGo, hitTargets, 1,
                              finish.targetFlags, finish.targetUnitGuid, missilePtr);

  if (auto map = runtime().GetMap(finish.mapId)) {
    if (finish.power1Delta != 0) {
      if (ApplyPlayerSpellPowerCostOnMap(finish.mapId, map, finish.casterGuid, finish.power1Delta)) {
        if (auto pl = map->TryGetPlayer(finish.casterGuid))
          _loginPower1 = pl->GetLivePower1();
      }
      BroadcastPlayerPower1OnMap(finish.mapId, map, finish.casterGuid,
                                 DefaultCasterPrimaryPowerType(_playerClass));
    }

    SpellCastOutcome combat{};
    combat.hasDirectHealthEffect = finish.hasDirectHealthEffect;
    combat.directHealthTargetGuid = finish.directHealthTargetGuid;
    combat.directHealthDelta = finish.directHealthDelta;
        combat.power1Delta = 0;
    combat.hasAuraApply = finish.hasAuraApply;
    combat.auraTargetGuid = finish.auraTargetGuid;
    combat.auraCasterGuid = finish.auraCasterGuid;
    combat.auraSpellId = finish.auraSpellId;
    combat.auraEffectType = finish.auraEffectType;
    combat.auraEffectIndex = finish.auraEffectIndex;
    combat.auraBasePoints = finish.auraBasePoints;
    combat.auraDieSides = finish.auraDieSides;
    combat.auraDurationMs = finish.auraDurationMs;
    combat.auraPeriodicPeriodMs = finish.auraPeriodicPeriodMs;
    combat.auraPeriodicHealthDeltaPerTick = finish.auraPeriodicHealthDeltaPerTick;
    combat.auraIsNegative = finish.auraIsNegative;
    combat.auraCasterLevel = finish.auraCasterLevel;
    map->BroadcastPacketToNearby(finish.casterGuid, spellGo, true);
    ApplySpellCastAuraOnMap(finish.mapId, map, combat, nowGo);
    (void)ApplySpellCastOutcomeOnMap(finish.mapId, map, finish.casterGuid, finish.spellId,
                                    combat, nowGo);
    TryAggroCreatureFromSpellDamage(combat.directHealthTargetGuid,
                                    combat.directHealthDelta);
    ScheduleSpellImpactVisual(map, finish.casterGuid, finish.spellId, finish.hitGuid,
                              finish.spellImpactDelayMs);
    if (finish.spellCategoryCooldownGroup > 0 &&
        finish.spellCategoryCooldownDurationMs > 0) {
      _spellCategoryCooldownUntil[finish.spellCategoryCooldownGroup] =
          nowGo + std::chrono::milliseconds(static_cast<int64_t>(
              finish.spellCategoryCooldownDurationMs));
  }
    CommitSpellRecoveryCooldownFromDeferred(finish.spellId, finish.spellCooldownDurationMs,
                                            nowGo);
    } else {
    SendPacket(spellGo);
  }
  }

void WorldSession::ScheduleSpellImpactVisual(std::shared_ptr<Map> map, uint64 casterGuid,
                                             uint32 spellId, uint64 hitTargetGuid,
                                             uint32 delayMs) {
  if (!map || hitTargetGuid == 0 || spellId == 0)
    return;

  auto sendImpact = [map, casterGuid, spellId, hitTargetGuid]() {
    BroadcastSpellImpactVisualOnMap(map, casterGuid, spellId, hitTargetGuid);
  };

  if (delayMs == 0u) {
    sendImpact();
    return;
  }

  auto self = shared_from_this();
  Asio::SpawnDetached(_socket.get_executor(),
                      [self, this, sendImpact = std::move(sendImpact),
                       delayMs]() -> Asio::awaitable<void> {
                        boost::asio::steady_timer timer(_socket.get_executor());
                        boost::system::error_code ec;
                        timer.expires_after(std::chrono::milliseconds(
                            static_cast<int64_t>(std::max<uint32_t>(1u, delayMs))));
                        co_await timer.async_wait(
                            boost::asio::redirect_error(Asio::use_awaitable, ec));
                        if (ec)
                          co_return;
    sendImpact();
                      });
  }

void WorldSession::HandleCastSpell(WorldPacket &packet) {
  if (_playerGuid == 0)
    return;
  if (!_spellManager) {
    LOG_ERROR("HandleCastSpell: SpellManager not configured");
    return;
  }

  CancelPendingClientSpellCast();

  SpellCastWire::ClientCastSpellData c;
  if (!SpellCastWire::TryReadClientCastSpell(packet, c)) {
    LOG_DEBUG(
        "CMSG_CAST_SPELL: unsupported tail or truncated packet (spellId={}, "
        "sendCastFlags=0x{:02X}, readPos={}/{})",
        c.spellId, static_cast<unsigned>(c.sendCastFlags), packet.GetReadPos(),
        packet.Size());
    return;
  }

  auto const now = std::chrono::steady_clock::now();
  SpellCastRequest req;
  req.casterGuid = _playerGuid;
  req.mapId = _mapId;
  req.client = c;
  req.now = now;
  req.gcdReady = _gcdReady;
  req.clientTimestampMs = _position.time;
  req.knownSpells = &_knownSpellIds;
  req.casterLevel = _playerLevel > 0 ? _playerLevel : 1;
  req.casterPrimaryPowerType = DefaultCasterPrimaryPowerType(_playerClass);
  MovementInfo const &pos = GetPosition();
  req.hasCasterWorldPosition = true;
  req.casterX = pos.x;
  req.casterY = pos.y;
  req.casterZ = pos.z;
  if (c.unitTargetGuid != 0) {
    if (c.unitTargetGuid == _playerGuid) {
      req.hasTargetWorldPosition = true;
      req.targetX = pos.x;
      req.targetY = pos.y;
      req.targetZ = pos.z;
    } else if (auto map = runtime().GetMap(_mapId)) {
      float tx = 0.f;
      float ty = 0.f;
      float tz = 0.f;
      if (map->TryGetObjectWorldPosition(c.unitTargetGuid, tx, ty, tz)) {
      req.hasTargetWorldPosition = true;
        req.targetX = tx;
        req.targetY = ty;
        req.targetZ = tz;
  }
      if (auto casterPl = map->TryGetPlayer(_playerGuid)) {
        if (auto targetPl = map->TryGetPlayer(c.unitTargetGuid)) {
          bool sameTeam = false;
          if (TrySpellRangeFriendlyTeamHint(casterPl->GetRace(), targetPl->GetRace(),
                                             &sameTeam)) {
            req.hasTargetFactionReactionHint = true;
            req.targetIsFriendlyTeamForSpellRange = sameTeam;
  }
  }
  }
  }
  }

  std::shared_ptr<IMapCollisionQueries> collisionHeld =
      runtime().GetCollisionQueries();
  if (collisionHeld)
    req.collisionQueries = collisionHeld.get();

  req.spellCooldownUntilBySpellId = &_spellCooldownUntil;
  req.spellCategoryCooldownUntilByGroup = &_spellCategoryCooldownUntil;
    if (auto map = runtime().GetMap(_mapId)) {
      if (auto casterPl = map->TryGetPlayer(_playerGuid)) {
      req.hasCasterPowerSnapshot = true;
      req.casterPower1 = casterPl->GetLivePower1();
      req.casterMaxPower1 = casterPl->GetLiveMaxPower1();
            req.casterBasePower1 = casterPl->GetLiveBasePower1();
  }
  }
    if (!req.hasCasterPowerSnapshot && _loginMaxPower1 > 0u) {
      req.hasCasterPowerSnapshot = true;
        req.casterPower1 = _loginPower1;
        req.casterMaxPower1 = _loginMaxPower1;
        req.casterBasePower1 = _loginMaxPower1;
  }

  SpellCastOutcome out;
  _spellManager->ProcessCastRequest(req, &out);

  switch (out.kind) {
  case SpellCastOutcome::Kind::SpellFailure:
    SendPacket(out.failurePacket);
    return;
  case SpellCastOutcome::Kind::SpellStartAndGo:
    if (auto map = runtime().GetMap(_mapId)) {
      map->BroadcastPacketToNearby(_playerGuid, out.spellStart, true);
      if (out.power1Delta != 0) {
        if (ApplyPlayerSpellPowerCostOnMap(_mapId, map, _playerGuid, out.power1Delta)) {
          if (auto pl = map->TryGetPlayer(_playerGuid))
            _loginPower1 = pl->GetLivePower1();
        }
        BroadcastPlayerPower1OnMap(_mapId, map, _playerGuid, req.casterPrimaryPowerType);
      }
      map->BroadcastPacketToNearby(_playerGuid, out.spellGo, true);
      ApplySpellCastAuraOnMap(_mapId, map, out, now);
            SpellCastOutcome combatOnly = out;
            combatOnly.power1Delta = 0;
      (void)ApplySpellCastOutcomeOnMap(_mapId, map, _playerGuid,
                                                                            static_cast<uint32>(c.spellId), combatOnly, now);
      TryAggroCreatureFromSpellDamage(out.directHealthTargetGuid, out.directHealthDelta);
      ScheduleSpellImpactVisual(map, _playerGuid, static_cast<uint32>(c.spellId),
                                out.primaryHitTargetGuid, out.spellImpactDelayMs);
      CommitSpellCooldownsFromCast(static_cast<uint32>(c.spellId), out, now);
    } else {
      SendPacket(out.spellStart);
      SendPacket(out.spellGo);
      CommitSpellCooldownsFromCast(static_cast<uint32>(c.spellId), out, now);
  }
    _gcdReady = out.newGcdReady;
    if (out.newGcdReady > now)
      _gcdTriggerSpellId = static_cast<uint32>(c.spellId);
    return;
  case SpellCastOutcome::Kind::SpellStartDeferred:
    if (auto map = runtime().GetMap(_mapId)) {
      map->BroadcastPacketToNearby(_playerGuid, out.spellStart, true);
    } else {
      SendPacket(out.spellStart);
  }
    _gcdReady = out.newGcdReady;
    if (out.newGcdReady > now)
      _gcdTriggerSpellId = static_cast<uint32>(c.spellId);
    SendClientSpellCooldownsAfterCast(static_cast<uint32>(c.spellId), 0u, out.newGcdReady,
                                      now);
    ScheduleDeferredSpellCastCompletion(out);
    return;
  case SpellCastOutcome::Kind::None:
  default:
    return;
  }
  }

void WorldSession::HandleCancelAura(WorldPacket &packet) {
  if (_playerGuid == 0)
    return;
  if (packet.Size() < sizeof(uint32))
    return;

  uint32 const spellId = packet.Read<uint32>();
  if (spellId == 0)
    return;

  if (_spellDefinitions) {
    if (auto def = _spellDefinitions->GetDefinition(spellId)) {
      if (!def->playerCanCancelAuraByClient())
    return;
  }
  }

  auto map = runtime().GetMap(_mapId);
  if (!map)
    return;

  if (!RemovePlayerAuraOnMap(_mapId, map, _playerGuid, spellId, _playerLevel))
    (void)RemoveAuraOnMapBySpellId(_mapId, map, spellId, _playerGuid);
  }

void WorldSession::HandleCancelCast(WorldPacket &packet) {
  if (_playerGuid == 0)
    return;

  uint32 spellId = 0;
  uint8 castId = 0;
  if (!SpellCastWire::TryReadClientCancelCast(packet, spellId, castId))
    return;

  bool const hadDeferred = _pendingDeferredCastActive;
  uint8 const pendingCastId = _pendingCastId;
  uint32 const pendingSpellId = _pendingSpellId;

  CancelPendingClientSpellCast();

  if (!hadDeferred)
    return;

  if (spellId != 0u && pendingSpellId != 0u && spellId != pendingSpellId)
    return;

  WorldPacket fail;
  SpellCastWire::BuildSpellFailure(
      fail, _playerGuid, castId != 0 ? castId : pendingCastId,
      static_cast<int32>(spellId != 0 ? spellId : pendingSpellId),
      SpellCastWire::SPELL_FAILED_INTERRUPTED);
  SendPacket(fail);
  }

} // namespace Firelands
