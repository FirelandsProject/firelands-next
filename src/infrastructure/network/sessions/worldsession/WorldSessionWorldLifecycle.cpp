#include <application/services/WorldService.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionMovementChecks.h>
#include <shared/Logger.h>
#include <shared/network/MovementStateQueries.h>
#include <shared/game/ActionButton.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <optional>

namespace Firelands {

void WorldSession::FinalizeWorldExit() {
  if (_playerGuid == 0 || _accountId == 0)
    return;

  CancelPeriodicTimeSync();
  ResetBreathMirrorState();
  CancelPendingClientSpellCast();

  if (_awaitingTeleportNear) {
    _position.x = _teleportPendingX;
    _position.y = _teleportPendingY;
    _position.z = _teleportPendingZ;
    _position.orientation = _teleportPendingO;
    _position.flags = 0;
    _position.flags2 = 0;
    _position.time = 0;
    _position.fallTime = 0;
    _awaitingTeleportNear = false;
  }

  uint64 const guid = _playerGuid;
  uint32 const mapId = _mapId;

  uint32 const charGuidLow = static_cast<uint32>(guid);
  uint16 const mapIdDb =
      static_cast<uint16>(std::min<uint32_t>(_mapId, 0xFFFFu));
  uint16 const zoneIdDb =
      static_cast<uint16>(std::min<uint32_t>(_zoneId, 0xFFFFu));
  MovementInfo persistPos = _position;
  if (!WsIsSaneWorldPosition(persistPos)) {
    if (auto ch = _charService->GetCharacterByGuid(guid)) {
      persistPos.x = ch->GetX();
      persistPos.y = ch->GetY();
      persistPos.z = ch->GetZ();
      persistPos.orientation = ch->GetOrientation();
    } else {
      persistPos = MovementInfo{};
    }
  }

  std::optional<uint32_t> liveHealth;
  std::optional<uint32_t> livePower1;
  if (auto map = WorldService::Instance().GetMap(mapId)) {
    if (auto pl = map->TryGetPlayer(guid)) {
      liveHealth = pl->GetLiveHealth();
      livePower1 = pl->GetLivePower1();
    }
  }

  SavePersistedSpellCooldowns(charGuidLow);
  SaveActionButtonsForCharacter(charGuidLow);

  if (!_charService->SaveCharacterOnLogout(
          _accountId, charGuidLow, mapIdDb, zoneIdDb, persistPos.x, persistPos.y,
          persistPos.z, persistPos.orientation, _moneyCopper, _playerXp, _tutorialInts,
          liveHealth, livePower1)) {
    LOG_ERROR("SaveCharacterOnLogout failed for guid {}, account {}",
              charGuidLow, _accountId);
  } else {
    LOG_DEBUG("Saved logout position guid {}: map {} zone {} x={} y={} z={} o={}",
            charGuidLow, mapIdDb, zoneIdDb, persistPos.x, persistPos.y,
            persistPos.z, persistPos.orientation);
  }

  auto ch = _charService->GetCharacterByGuid(charGuidLow);
  if (ch) {
    auto invData = ch->GetBag0Inventory();
    if (!_charService->SaveInventory(charGuidLow, invData)) {
      LOG_ERROR("SaveInventory failed for guid {}, account {}",
                charGuidLow, _accountId);
    } else {
      LOG_DEBUG("Saved inventory for guid {}", charGuidLow);
    }
  }

  if (auto host = WorldService::Instance().GetScriptHost()) {
    host->FireEvent("player_logout", guid);
  }

  WorldService::Instance().RemovePlayerFromMap(mapId, guid);

  UnregisterFromOnlineCharacterRegistryIfNeeded();

  _playerGuid = 0;
  _clientSelectionGuid = 0;
  _activeCharacterGuid = 0;
  _playerRace = 0;
  _playerClass = 0;
  _playerXp = 0;
  _sentOpeningCinematic = false;
  _tutorialInts.fill(0);
  _knownSpells.clear();
  _knownSpellIds.clear();
  _knownSkills.clear();
  _gcdReady = {};
  _gcdTriggerSpellId = 0;
  _spellCooldownUntil.clear();
  _spellCategoryCooldownUntil.clear();
  for (auto &bar : _actionButtonBySpec)
    ActionButton::ClearBar(bar);
  _activeActionBarSpec = 0;
  _actionBarToggles = 0xFF;
  ResetGmStateForLogout();
  _mapId = 0;
  _zoneId = 0;
  _timeSyncNextCounter = 0;
  _position = MovementInfo{};
}

void WorldSession::HandleLogoutRequest(WorldPacket & /*packet*/) {
  if (_playerGuid == 0) {
    LOG_WARN("CMSG_LOGOUT_REQUEST ignored (not in world)");
    return;
  }

  // Cataclysm 4.3.4 order: uint32 reason (0 = OK), uint8 instantLogout.
  // We always allow instant logout (no combat/rest model yet); client returns to
  // character selection after SMSG_LOGOUT_COMPLETE.
  WorldPacket response(SMSG_LOGOUT_RESPONSE, 5);
  response.Append<uint32>(0); // reason
  response.Append<uint8>(1);  // instant logout
  SendPacket(response);

  uint64 const guid = _playerGuid;
  FinalizeWorldExit();

  WorldPacket complete(SMSG_LOGOUT_COMPLETE, 0);
  SendPacket(complete);
  LOG_INFO("Logout: Account={} GUID={} (returned to character select)", _accountId, guid);
}

void WorldSession::HandleLogoutCancel(WorldPacket & /*packet*/) {
  // Only meaningful during a timed logout; instant logout never reaches this.
  if (_playerGuid == 0)
    return;

  WorldPacket ack(SMSG_LOGOUT_CANCEL_ACK, 0);
  SendPacket(ack);
}

} // namespace Firelands
