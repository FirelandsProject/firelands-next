#include <application/services/WorldService.h>
#include <domain/world/Player.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/game/PlayerGmAppearance.h>
#include <shared/network/MovementSetPackets.h>
#include <shared/network/MovementStateQueries.h>
#include <shared/network/UpdateFields.h>
#include <shared/network/UpdateData.h>
#include <cmath>
#include <map>

namespace Firelands {

namespace {

constexpr float kDefaultRunSpeed = 7.0f;
constexpr float kMinRunSpeed = 0.5f;
constexpr float kMaxRunSpeed = 50.0f;

} // namespace

void WorldSession::ResetGmStateForLogout() {
  _gmAppearance = {};
  _gmFlyEnabled = false;
  _gmRunSpeed = kDefaultRunSpeed;
  _moveCounterForGmPackets = 0;
  _forcedFactionReactions.clear();
}

void WorldSession::SetGmTagEnabled(bool on) {
  bool const wasOn = _gmAppearance.gmTagOn;
  _gmAppearance.gmTagOn = on;
  PublishGmVisualPatchIfInWorld();
  if (_playerGuid == 0 || wasOn == on)
    return;

  // Sync GM mode flag on the Player for creature aggro immunity
  if (auto map = runtime().GetMap(_mapId)) {
    if (auto player = map->TryGetPlayer(_playerGuid))
      player->SetGmModeEnabled(on);
  }

  if (on) {
    LOG_DEBUG("GM mode enabled: clearing active creature combat for playerGuid={} mapId={}",
              _playerGuid, _mapId);
    StopAllCreatureCombat(false);
    StopMeleeAutoAttack(false);
  }

  RefreshNearbyCreaturePhaseVisibility(_position.x, _position.y);
  RefreshNearbyCreatureGmWireFlags();
}

void WorldSession::SetDndEnabled(bool on) {
  _gmAppearance.dndOn = on;
  PublishGmVisualPatchIfInWorld();
}

void WorldSession::SetDevTagEnabled(bool on) {
  _gmAppearance.devTagOn = on;
  PublishGmVisualPatchIfInWorld();
}

void WorldSession::SetGmVisibleToPlayers(bool visible) {
  _gmAppearance.visibleToOthers = visible;
  PublishGmVisualPatchIfInWorld();
}

void WorldSession::SetGmFlyEnabled(bool on) {
  _gmFlyEnabled = on;
  if (_playerGuid != 0) {
    ApplyGmFlyAuthority(_position, _gmFlyEnabled, _breathMirrorActive);
    _movementAnimTierSent.reset();
    bool const inLiquid =
        MovementIsSwimming(_position) || _breathMirrorActive;
    UpdateBreathFromSwimmingState(inLiquid);
    SyncPlayerMovementHintsIfNeeded(inLiquid);
    if (auto map = runtime().GetMap(_mapId))
      map->UpdateObjectPosition(_playerGuid, _position);
  }
  PublishGmMovementPacketsIfInWorld();
}

void WorldSession::SetGmRunSpeed(float speed) {
  if (!std::isfinite(speed))
    return;
  if (speed < kMinRunSpeed)
    speed = kMinRunSpeed;
  else if (speed > kMaxRunSpeed)
    speed = kMaxRunSpeed;
  _gmRunSpeed = speed;
  PublishGmMovementPacketsIfInWorld();
}

void WorldSession::PublishGmVisualPatchIfInWorld() {
  if (_playerGuid == 0)
    return;
  auto ch = _charService->GetCharacterByGuid(_playerGuid);
  if (!ch)
    return;

  std::map<uint16, uint32> patch;
  uint32 pf = 0;
  if (_gmAppearance.gmTagOn)
    pf |= PLAYER_FLAGS_GM_TAG;
  if (_gmAppearance.dndOn)
    pf |= PLAYER_FLAGS_DND;
  if (_gmAppearance.devTagOn)
    pf |= PLAYER_FLAGS_DEVELOPER;
  patch[static_cast<uint16>(PLAYER_FLAGS)] = pf;

  uint32 uf = 0;
  if (!_gmAppearance.visibleToOthers)
    uf |= UNIT_FLAG_INVISIBLE;
  patch[static_cast<uint16>(UNIT_FIELD_FLAGS)] = uf;

  UpdateData update(_mapId);
  update.AddValuesUpdate(_playerGuid, patch);
  WorldPacket pkt;
  update.Build(pkt);
  if (auto map = runtime().GetMap(_mapId))
    map->BroadcastPacketToNearby(_playerGuid, pkt, true);
  else
    SendPacket(pkt);
}

void WorldSession::PublishGmMovementPacketsIfInWorld() {
  if (_playerGuid == 0)
    return;

  WorldPacket run = BuildSmsgMoveSetRunSpeed(_playerGuid, ++_moveCounterForGmPackets,
                                             _gmRunSpeed);
  WorldPacket flight = BuildSmsgMoveSetFlightSpeed(
      _playerGuid, ++_moveCounterForGmPackets, _gmRunSpeed);
  WorldPacket fly =
      _gmFlyEnabled ? BuildSmsgMoveSetCanFly(_playerGuid, ++_moveCounterForGmPackets)
                   : BuildSmsgMoveUnsetCanFly(_playerGuid, ++_moveCounterForGmPackets);

  if (auto map = runtime().GetMap(_mapId)) {
    map->BroadcastPacketToNearby(_playerGuid, run, true);
    map->BroadcastPacketToNearby(_playerGuid, flight, true);
    map->BroadcastPacketToNearby(_playerGuid, fly, true);
  } else {
    SendPacket(run);
    SendPacket(flight);
    SendPacket(fly);
  }
}

} // namespace Firelands
