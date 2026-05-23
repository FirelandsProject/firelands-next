#include <application/services/WorldService.h>
#include <domain/world/Map.h>
#include <domain/world/Player.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/dbc/EmotesTextDbc.h>
#include <shared/game/Emotes.h>
#include <shared/Logger.h>
#include <shared/network/MovementStateQueries.h>
#include <shared/network/WorldOpcodes.h>

namespace Firelands {

std::string WorldSession::ResolveTextEmoteTargetName(uint64_t targetGuid) const {
  if (targetGuid == 0)
    return {};
  if (auto ch = _charService->GetCharacterByGuid(targetGuid))
    return ch->GetName();
    return {};
}

bool WorldSession::IsActivePlayerAlive() const {
  if (_playerGuid == 0)
    return false;
  if (auto map = runtime().GetMap(_mapId)) {
    if (auto pl = map->TryGetPlayer(_playerGuid))
      return pl->GetLiveHealth() > 0;
}
  return true;
}

void WorldSession::TryClearEmotesOnMovement(WorldOpcode opcode,
                                            bool positionChanged) {
  if (_unitNpcEmoteState == 0)
    return;

  bool const moving = MovementIsMoving(_position) ||
                      MovementIsMovingHorizontally(_position) ||
                      MovementIsTurning(_position) || MovementIsFalling(_position);

  bool const explicitMotion = [&opcode]() {
    switch (opcode) {
    case MSG_MOVE_START_FORWARD:
    case MSG_MOVE_START_BACKWARD:
    case MSG_MOVE_START_STRAFE_LEFT:
    case MSG_MOVE_START_STRAFE_RIGHT:
    case MSG_MOVE_START_ASCEND:
    case MSG_MOVE_START_DESCEND:
    case MSG_MOVE_JUMP:
    case MSG_MOVE_SET_FACING:
    case MSG_MOVE_FALL_LAND:
    case MSG_MOVE_START_SWIM:
  return true;
    default:
    return false;
}
  }();

  if (!moving && !explicitMotion && !positionChanged)
    return;

  ApplyUnitNpcEmoteState(0);
}

void WorldSession::ApplyUnitNpcEmoteState(uint32_t emoteAnim) {
  if (_playerGuid == 0)
    return;
  if (_unitNpcEmoteState == emoteAnim)
    return;
  _unitNpcEmoteState = emoteAnim;

  WorldPacket updatePkt;
  WorldSessionObjectUpdate::BuildUnitNpcEmoteStateValuesUpdate(
      static_cast<uint16>(_mapId), _playerGuid, emoteAnim, updatePkt);
  if (auto map = runtime().GetMap(_mapId))
    map->BroadcastPacketToNearby(_playerGuid, updatePkt, true);
  else
    SendPacket(updatePkt);
}

void WorldSession::BroadcastEmoteAnimation(uint32_t emoteAnim) {
  if (_playerGuid == 0)
    return;

  WorldPacket pkt(static_cast<uint32>(SMSG_EMOTE), 12);
  pkt.Append<uint32>(emoteAnim);
  pkt.Append<uint64>(_playerGuid);

  if (auto map = runtime().GetMap(_mapId))
    map->BroadcastPacketToNearby(_playerGuid, pkt, true);
  else
    SendPacket(pkt);
}

void WorldSession::BroadcastTextEmote(uint32_t textEmote, uint32_t emoteNum,
                                      uint64_t targetGuid) {
  if (_playerGuid == 0)
    return;

  std::string const targetName = ResolveTextEmoteTargetName(targetGuid);
  uint32_t const nameLen = static_cast<uint32_t>(targetName.size());

  WorldPacket pkt(static_cast<uint32>(SMSG_TEXT_EMOTE),
                  20 + targetName.size() + 4);
  pkt.Append<uint64>(_playerGuid);
  pkt.Append<uint32>(textEmote);
  pkt.Append<uint32>(emoteNum);
  pkt.Append<uint32>(nameLen);
  if (nameLen > 1)
    pkt.WriteString(targetName);
  else
    pkt.Append<uint8>(0);

  if (auto map = runtime().GetMap(_mapId))
    map->BroadcastPacketToNearby(_playerGuid, pkt, true);
  else
    SendPacket(pkt);
}

void WorldSession::HandleEmoteOpcode(WorldPacket &packet) {
  if (_playerGuid == 0 || !IsActivePlayerAlive())
    return;
  if (packet.Size() < sizeof(uint32))
    return;

  uint32_t const emote = packet.Read<uint32>();
  // Client only hardcodes cancel + wave stop (`HandleEmoteOpcode`).
  if (emote != EMOTE_ONESHOT_NONE && emote != EMOTE_ONESHOT_WAVE)
    return;

  if (_unitNpcEmoteState != 0)
  ApplyUnitNpcEmoteState(0);
}

void WorldSession::HandleTextEmoteOpcode(WorldPacket &packet) {
  if (_playerGuid == 0 || !IsActivePlayerAlive())
    return;
  if (packet.Size() < sizeof(uint32) * 2 + sizeof(uint64))
    return;

  uint32_t const textEmote = packet.Read<uint32>();
  uint32_t const emoteNum = packet.Read<uint32>();
  uint64_t const targetGuid = packet.Read<uint64>();

  if (!_emotesTextDbc) {
    LOG_DEBUG("CMSG_TEXT_EMOTE id={} ignored (EmotesText.dbc not loaded)",
              textEmote);
    return;
}

  std::optional<uint32_t> const animOpt =
      _emotesTextDbc->LookupEmoteAnim(textEmote);
  if (!animOpt) {
    LOG_DEBUG("CMSG_TEXT_EMOTE unknown text emote id {}", textEmote);
    return;
}

  uint32_t const emoteAnim = *animOpt;

  switch (emoteAnim) {
  case EMOTE_STATE_SLEEP:
  case EMOTE_STATE_SIT:
  case EMOTE_STATE_KNEEL:
  case EMOTE_ONESHOT_NONE:
    break;
  case EMOTE_STATE_DANCE:
  case EMOTE_STATE_READ:
    ApplyUnitNpcEmoteState(emoteAnim);
    break;
    default:
    BroadcastEmoteAnimation(emoteAnim);
    break;
}

  BroadcastTextEmote(textEmote, emoteNum, targetGuid);
}

} // namespace Firelands
