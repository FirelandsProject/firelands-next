#include <application/services/CharacterActionButtons.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/Logger.h>
#include <shared/game/ActionButton.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <shared/network/packets/client/PackedPlayerGuidWire.h>

namespace Firelands {

void WorldSession::LoadActionButtonsForCharacter(uint32_t characterGuid) {
  for (auto &bar : _actionButtonBySpec)
    ActionButton::ClearBar(bar);

  for (uint8_t spec = 0; spec < ActionButton::kMaxActionBarSpecs; ++spec) {
    CharacterActionButtonState const loaded =
        _charService->LoadCharacterActionButtons(characterGuid, spec);
    ApplyPersistedActionButtons(_actionButtonBySpec[spec], loaded);
    if (!loaded.buttons.empty()) {
      LOG_INFO("Loaded {} action bar slot(s) for guid {} spec {}",
               loaded.buttons.size(), characterGuid, static_cast<unsigned>(spec));
    }
  }
}

void WorldSession::SaveActionButtonsForCharacter(uint32_t characterGuid) {
  for (uint8_t spec = 0; spec < ActionButton::kMaxActionBarSpecs; ++spec) {
    CharacterActionButtonState const state =
        BuildPersistedActionButtons(_actionButtonBySpec[spec], spec);
    if (!_charService->SaveCharacterActionButtons(characterGuid, spec, state)) {
      LOG_WARN("SaveCharacterActionButtons failed for guid {} spec {}", characterGuid,
               static_cast<unsigned>(spec));
      return;
    }
    LOG_DEBUG("Saved {} action bar slot(s) for guid {} spec {}", state.buttons.size(),
              characterGuid, static_cast<unsigned>(spec));
  }
  if (!_charService->UpdateCharacterActionBarToggles(characterGuid, _actionBarToggles))
    LOG_WARN("UpdateCharacterActionBarToggles failed for guid {}", characterGuid);
}

void WorldSession::HandleObjectUpdateFailed(WorldPacket &packet) {
  if (_playerGuid == 0 || packet.Size() == 0)
    return;
  // Cataclysm 4.3.4: bit-packed ObjectGuid (same as CMSG_PLAYER_LOGIN), not legacy mask+bytes.
  uint64 failedGuid = 0;
  WorldPackets::Client::ReadLoginPackedPlayerGuid(packet, failedGuid);
  if (failedGuid == 0)
    return;
  if (failedGuid == _playerGuid) {
    LOG_WARN("CMSG_OBJECT_UPDATE_FAILED for self guid {} (client rejected player "
             "UPDATE_OBJECT; check login burst)",
             _playerGuid);
  } else {
    LOG_DEBUG("CMSG_OBJECT_UPDATE_FAILED guid {}", failedGuid);
  }
}

void WorldSession::HandleSetActionBarToggles(WorldPacket &packet) {
  if (packet.Size() < 1) {
    LOG_WARN("CMSG_SET_ACTIONBAR_TOGGLES: empty payload");
    return;
  }

  uint8_t const mask = packet.Read<uint8_t>();
  if (_playerGuid == 0) {
    LOG_WARN(
        "CMSG_SET_ACTIONBAR_TOGGLES ignored (not in world); mask=0x{:02X}. "
        "Change action bars in Interface while logged in on the character, then "
        "click Okay.",
        static_cast<unsigned>(mask));
    return;
  }

  if (mask == _actionBarToggles) {
    LOG_DEBUG("Action bar toggles unchanged guid={} mask=0x{:02X}",
              static_cast<uint32_t>(_playerGuid), static_cast<unsigned>(mask));
    return;
  }

  _actionBarToggles = mask;

  uint32_t const charGuid = static_cast<uint32_t>(_playerGuid);
  if (!_charService->UpdateCharacterActionBarToggles(charGuid, mask))
    LOG_WARN("UpdateCharacterActionBarToggles failed guid {}", charGuid);
  else
    LOG_INFO("Action bar toggles saved guid={} mask=0x{:02X} (255=all shown, 0=all "
             "hidden)",
             charGuid, static_cast<unsigned>(mask));

  SendActionBarTogglesUpdate();
}

void WorldSession::HandleLoadingScreenNotify(WorldPacket &packet) {
  if (_playerGuid == 0)
    return;
  // Cataclysm 4.3.4: uint32 mapId, uint8 isShowing (non-zero while loading screen up).
  if (packet.Size() < 5)
    return;

  uint32_t const mapId = packet.Read<uint32_t>();
  uint8_t const isShowing = packet.Read<uint8_t>();
  if (isShowing != 0)
    return;

  SendActionBarTogglesUpdate();
  LOG_DEBUG("Action bar toggles re-sent after loading screen map={} mask=0x{:02X}",
            mapId, static_cast<unsigned>(_actionBarToggles));
}

void WorldSession::SendActionBarTogglesUpdate() {
  if (_playerGuid == 0)
    return;
  WorldPacket out;
  WorldSessionObjectUpdate::BuildPlayerActionBarTogglesValuesUpdate(
      static_cast<uint16>(_mapId), _playerGuid, _actionBarToggles, out);
  if (out.Size() > 0)
    SendPacket(out);
}

void WorldSession::HandleSetActionButton(WorldPacket &packet) {
  if (_playerGuid == 0)
    return;

  ActionButton::SetActionButtonCmsg req;
  if (!ActionButton::TryParseSetActionButtonCmsg(packet, req)) {
    LOG_WARN("CMSG_SET_ACTION_BUTTON: unparseable payload (size={})", packet.Size());
    return;
  }

  uint32_t const charGuid = static_cast<uint32_t>(_playerGuid);
  uint8_t const spec = _activeActionBarSpec;
  ActionButton::PackedActionBar &bar = ActiveActionBar();

  if (req.packedAction == 0) {
    bar[req.index] = 0;
    if (!_charService->DeleteCharacterActionButton(charGuid, spec, req.index))
      LOG_WARN("DeleteCharacterActionButton failed guid {} spec {} slot {}", charGuid,
               static_cast<unsigned>(spec), req.index);
    return;
  }

  uint32_t const action = ActionButton::ActionFromPacked(req.packedAction);
  uint8_t const type = ActionButton::TypeFromPacked(req.packedAction);

  auto ch = _charService->GetCharacterByGuid(_playerGuid);
  if (!ch) {
    LOG_WARN("CMSG_SET_ACTION_BUTTON: character {} not found", _playerGuid);
    return;
  }

  if (!IsValidActionButtonPlacement(type, action, _knownSpellIds, *ch)) {
    LOG_WARN("CMSG_SET_ACTION_BUTTON rejected type={} action={} slot={}", type, action,
             req.index);
    return;
  }

  bar[req.index] = ActionButton::PackWire(action, type);
  if (!_charService->UpsertCharacterActionButton(charGuid, spec, req.index, action, type))
    LOG_WARN("UpsertCharacterActionButton failed guid {} spec {} slot {}", charGuid,
             static_cast<unsigned>(spec), req.index);
  else
    LOG_INFO("Action bar saved: guid={} spec={} slot={} action={} type={}", charGuid,
             static_cast<unsigned>(spec), req.index, action, type);
}

} // namespace Firelands
