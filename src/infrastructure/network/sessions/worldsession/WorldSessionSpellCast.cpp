#include <application/ports/IMapCollisionQueries.h>
#include <application/services/WorldService.h>
#include <boost/asio/redirect_error.hpp>
#include <infrastructure/network/asio/AsioAwaitables.h>
#include <application/spell/SpellManager.h>
#include <domain/world/Creature.h>
#include <domain/world/Player.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/Logger.h>
#include <shared/game/PlayerFactionTeam.h>
#include <shared/network/SpellCastWire.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>

namespace Firelands {

namespace {

namespace ws_obj = WorldSessionObjectUpdate;

} // namespace

void WorldSession::CancelPendingClientSpellCast() {
  (void)_pendingSpellCastTimer.cancel();
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

  uint32 const castFlagsGo = SpellCastWire::CAST_FLAG_UNKNOWN_9;
  auto const nowGo = std::chrono::steady_clock::now();
  uint32 const castTimeGo = static_cast<uint32>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          nowGo.time_since_epoch())
          .count());
  WorldPacket spellGo;
  uint64 const hitTargets[1] = {finish.hitGuid};
  SpellCastWire::BuildSpellGo(spellGo, finish.casterGuid, finish.castId, finish.spellId,
                              castFlagsGo, 0, castTimeGo, hitTargets, 1,
                              finish.targetFlags, finish.targetUnitGuid);

  if (auto map = WorldService::Instance().GetMap(finish.mapId)) {
    map->BroadcastPacketToNearby(finish.casterGuid, spellGo, true);
    if (finish.hasDirectHealthEffect && finish.directHealthDelta != 0) {
      if (auto target = map->TryGetPlayer(finish.directHealthTargetGuid)) {
        target->ApplyHealthDelta(finish.directHealthDelta);
        WorldPacket hpUpdate;
        ws_obj::BuildPlayerHealthValuesUpdate(
            static_cast<uint16>(finish.mapId), finish.directHealthTargetGuid,
            target->GetLiveHealth(), target->GetLiveMaxHealth(), hpUpdate);
        map->BroadcastPacketToNearby(finish.directHealthTargetGuid, hpUpdate, true);
      } else if (auto cr = map->TryGetCreature(finish.directHealthTargetGuid)) {
        cr->ApplyHealthDelta(finish.directHealthDelta);
        WorldPacket hpUpdate;
        ws_obj::BuildPlayerHealthValuesUpdate(
            static_cast<uint16>(finish.mapId), finish.directHealthTargetGuid,
            cr->GetLiveHealth(), cr->GetLiveMaxHealth(), hpUpdate);
        map->BroadcastPacketToNearby(finish.directHealthTargetGuid, hpUpdate, true);
      }
    }
    if (finish.power1Delta != 0) {
      if (auto casterPl = map->TryGetPlayer(finish.casterGuid)) {
        casterPl->ApplyPower1Delta(finish.power1Delta);
        WorldPacket pwUpdate;
        ws_obj::BuildPlayerPower1ValuesUpdate(
            static_cast<uint16>(finish.mapId), finish.casterGuid,
            casterPl->GetLivePower1(), casterPl->GetLiveMaxPower1(), pwUpdate);
        map->BroadcastPacketToNearby(finish.casterGuid, pwUpdate, true);
      }
    }
    if (finish.spellCooldownDurationMs > 0) {
      _spellCooldownUntil[finish.spellId] =
          nowGo + std::chrono::milliseconds(
                      static_cast<int64_t>(finish.spellCooldownDurationMs));
    }
    if (finish.spellCategoryCooldownGroup > 0 &&
        finish.spellCategoryCooldownDurationMs > 0) {
      _spellCategoryCooldownUntil[finish.spellCategoryCooldownGroup] =
          nowGo + std::chrono::milliseconds(static_cast<int64_t>(
              finish.spellCategoryCooldownDurationMs));
    }
  } else {
    SendPacket(spellGo);
  }
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
  req.knownSpells = &_knownSpellIds;
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
    } else if (auto map = WorldService::Instance().GetMap(_mapId)) {
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
      WorldService::Instance().GetCollisionQueries();
  if (collisionHeld)
    req.collisionQueries = collisionHeld.get();

  req.spellCooldownUntilBySpellId = &_spellCooldownUntil;
  req.spellCategoryCooldownUntilByGroup = &_spellCategoryCooldownUntil;
  if (auto map = WorldService::Instance().GetMap(_mapId)) {
    if (auto casterPl = map->TryGetPlayer(_playerGuid)) {
      req.hasCasterPowerSnapshot = true;
      req.casterPower1 = casterPl->GetLivePower1();
      req.casterMaxPower1 = casterPl->GetLiveMaxPower1();
    }
  }

  SpellCastOutcome out;
  _spellManager->ProcessCastRequest(req, &out);

  switch (out.kind) {
  case SpellCastOutcome::Kind::SpellFailure:
    SendPacket(out.failurePacket);
    return;
  case SpellCastOutcome::Kind::SpellStartAndGo:
    if (auto map = WorldService::Instance().GetMap(_mapId)) {
      map->BroadcastPacketToNearby(_playerGuid, out.spellStart, true);
      map->BroadcastPacketToNearby(_playerGuid, out.spellGo, true);
      if (out.hasDirectHealthEffect && out.directHealthDelta != 0) {
        if (auto target = map->TryGetPlayer(out.directHealthTargetGuid)) {
          target->ApplyHealthDelta(out.directHealthDelta);
          WorldPacket hpUpdate;
          ws_obj::BuildPlayerHealthValuesUpdate(
              static_cast<uint16>(_mapId), out.directHealthTargetGuid,
              target->GetLiveHealth(), target->GetLiveMaxHealth(), hpUpdate);
          map->BroadcastPacketToNearby(out.directHealthTargetGuid, hpUpdate,
                                       true);
        } else if (auto cr = map->TryGetCreature(out.directHealthTargetGuid)) {
          cr->ApplyHealthDelta(out.directHealthDelta);
          WorldPacket hpUpdate;
          ws_obj::BuildPlayerHealthValuesUpdate(
              static_cast<uint16>(_mapId), out.directHealthTargetGuid,
              cr->GetLiveHealth(), cr->GetLiveMaxHealth(), hpUpdate);
          map->BroadcastPacketToNearby(out.directHealthTargetGuid, hpUpdate,
                                       true);
        }
      }
      if (out.power1Delta != 0) {
        if (auto casterPl = map->TryGetPlayer(_playerGuid)) {
          casterPl->ApplyPower1Delta(out.power1Delta);
          WorldPacket pwUpdate;
          ws_obj::BuildPlayerPower1ValuesUpdate(
              static_cast<uint16>(_mapId), _playerGuid, casterPl->GetLivePower1(),
              casterPl->GetLiveMaxPower1(), pwUpdate);
          map->BroadcastPacketToNearby(_playerGuid, pwUpdate, true);
        }
      }
      if (out.spellCooldownDurationMs > 0) {
        uint32 const sid = static_cast<uint32>(c.spellId);
        _spellCooldownUntil[sid] =
            now + std::chrono::milliseconds(
                      static_cast<int64_t>(out.spellCooldownDurationMs));
      }
      if (out.spellCategoryCooldownGroup > 0 &&
          out.spellCategoryCooldownDurationMs > 0) {
        _spellCategoryCooldownUntil[out.spellCategoryCooldownGroup] =
            now + std::chrono::milliseconds(static_cast<int64_t>(
                out.spellCategoryCooldownDurationMs));
      }
    } else {
      SendPacket(out.spellStart);
      SendPacket(out.spellGo);
    }
    _gcdReady = out.newGcdReady;
    return;
  case SpellCastOutcome::Kind::SpellStartDeferred:
    if (auto map = WorldService::Instance().GetMap(_mapId)) {
      map->BroadcastPacketToNearby(_playerGuid, out.spellStart, true);
    } else {
      SendPacket(out.spellStart);
    }
    _gcdReady = out.newGcdReady;
    ScheduleDeferredSpellCastCompletion(out);
    return;
  case SpellCastOutcome::Kind::None:
  default:
    return;
  }
}

} // namespace Firelands
