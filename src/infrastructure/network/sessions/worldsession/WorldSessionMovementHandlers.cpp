#include <application/services/WorldService.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionMovementChecks.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/game/PlayerGmAppearance.h>
#include <shared/Logger.h>
#include <shared/network/BitReader.h>
#include <shared/network/MovementFlags.h>
#include <shared/network/MovementStateQueries.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/MovementTeleportPackets.h>
#include <shared/network/MovementWire.h>
#include <shared/network/movement/ClientMovementMse.h>

namespace Firelands {

namespace ws_obj = WorldSessionObjectUpdate;

void WorldSession::SyncPlayerMovementHintsIfNeeded(bool inLiquidForBreath) {
  if (_playerGuid == 0)
    return;

  uint8 const tier = MovementAnimTierForLiquid(_position, inLiquidForBreath);
  if (_movementAnimTierSent.has_value() && *_movementAnimTierSent == tier)
    return;
  _movementAnimTierSent = tier;

  WorldPacket pkt;
  ws_obj::BuildPlayerMovementHintsValuesUpdate(
      static_cast<uint16>(_mapId), _playerGuid, _position,
      GetGmAppearanceForPlayerUpdates(), pkt);
  SendPacket(pkt);
  if (auto map = runtime().GetMap(_mapId))
    map->BroadcastPacketToNearby(_playerGuid, pkt, true);
}

void WorldSession::TeleportTo(uint32 mapId, float x, float y, float z,
                              float orientation) {
  if (_playerGuid == 0)
    return;

  if (mapId != _mapId) {
    SendNotification(
        "Cross-map teleport is not implemented yet; stay on your current map.");
    return;
  }

  if (_awaitingTeleportNear) {
    SendNotification("A teleport is already in progress.");
    return;
  }

  uint32 const seq = ++_moveCounterForGmPackets;
  _teleportAckExpectedIndex = seq;
  _teleportPendingX = x;
  _teleportPendingY = y;
  _teleportPendingZ = z;
  _teleportPendingO = orientation;
  _awaitingTeleportNear = true;

  WorldPacket teleport =
      BuildMsgMoveTeleport(_playerGuid, seq, x, y, z, orientation);
  SendPacket(teleport);

  SendNotification("Teleported to: " + std::to_string(x) + ", " +
                   std::to_string(y) + ", " + std::to_string(z));
}

void WorldSession::HandleMoveTeleportAck(WorldPacket &packet) {
  if (!_awaitingTeleportNear || _playerGuid == 0)
    return;

  if (packet.Size() < sizeof(int32) * 2 + 1u) {
    LOG_WARN("[MOVE] MSG_MOVE_TELEPORT_ACK too short ({} bytes)", packet.Size());
    _awaitingTeleportNear = false;
    return;
  }

  int32 const ackIndex = packet.Read<int32>();
  (void)packet.Read<int32>(); // moveTime

  BitReader br(packet);
  uint8 mask[8] = {};
  mask[5] = static_cast<uint8>(br.ReadBit());
  mask[0] = static_cast<uint8>(br.ReadBit());
  mask[1] = static_cast<uint8>(br.ReadBit());
  mask[6] = static_cast<uint8>(br.ReadBit());
  mask[3] = static_cast<uint8>(br.ReadBit());
  mask[7] = static_cast<uint8>(br.ReadBit());
  mask[2] = static_cast<uint8>(br.ReadBit());
  mask[4] = static_cast<uint8>(br.ReadBit());

  auto readSeqByte = [&](int idx) -> uint8 {
    if (mask[idx])
      return static_cast<uint8>(packet.Read<uint8>() ^ 1u);
    return 0;
  };

  uint8 const g4 = readSeqByte(4);
  uint8 const g2 = readSeqByte(2);
  uint8 const g7 = readSeqByte(7);
  uint8 const g6 = readSeqByte(6);
  uint8 const g5 = readSeqByte(5);
  uint8 const g1 = readSeqByte(1);
  uint8 const g3 = readSeqByte(3);
  uint8 const g0 = readSeqByte(0);

  uint64 const moverGuid =
      static_cast<uint64>(g0) | (static_cast<uint64>(g1) << 8) |
      (static_cast<uint64>(g2) << 16) | (static_cast<uint64>(g3) << 24) |
      (static_cast<uint64>(g4) << 32) | (static_cast<uint64>(g5) << 40) |
      (static_cast<uint64>(g6) << 48) | (static_cast<uint64>(g7) << 56);

  if (moverGuid != _playerGuid) {
    LOG_DEBUG("[MOVE] MSG_MOVE_TELEPORT_ACK for unexpected mover {}", moverGuid);
    _awaitingTeleportNear = false;
    return;
  }

  if (ackIndex != static_cast<int32>(_teleportAckExpectedIndex)) {
    LOG_DEBUG("[MOVE] MSG_MOVE_TELEPORT_ACK index mismatch: ack {} expected {}",
              ackIndex, _teleportAckExpectedIndex);
  }

  _awaitingTeleportNear = false;

  if (_unitNpcEmoteState != 0)
    ApplyUnitNpcEmoteState(0);

  _position.x = _teleportPendingX;
  _position.y = _teleportPendingY;
  _position.z = _teleportPendingZ;
  _position.orientation = _teleportPendingO;
  _position.flags = 0;
  _position.flags2 = 0;
  _position.time = 0;
  _position.fallTime = 0;

  if (auto map = runtime().GetMap(_mapId)) {
    map->UpdateObjectPosition(_playerGuid, _position);
    WorldPacket nearbyUpdate = BuildSmsgMoveUpdateTeleport(
        _playerGuid, _position.x, _position.y, _position.z, _position.orientation,
        _position.flags, _position.flags2, _position.time);
    map->BroadcastPacketToNearby(_playerGuid, nearbyUpdate, false);
  }

  ResetBreathMirrorState();
  _movementAnimTierSent.reset();
  SyncPlayerMovementHintsIfNeeded();

  // Login merges nearby creatures into the initial SMSG_UPDATE_OBJECT; teleport does not,
  // so the client would see an empty cell until we send CREATE for units here.
  SendNearbyCreatureCreatesToSelf(_teleportPendingX, _teleportPendingY);
}

void WorldSession::HandleMovement(WorldPacket &packet) {
  WorldOpcode const op = static_cast<WorldOpcode>(packet.GetOpcode());
  MovementInfo move{};
  bool const parsed = TryReadClientMovement(packet, op, move);

  // After logout the client may still send movement while transitioning to character
  // select. Echoing those packets breaks that transition (stuck loading).
  if (_playerGuid == 0)
    return;

  if (_awaitingTeleportNear)
    return;

  bool const trustFullPosition =
      parsed && WsIsTrustedPositionOpcode(op) && WsIsSaneWorldPosition(move);

  bool const mergeMovementState =
      parsed && !WsIsTrustedPositionOpcode(op) && WsIsClientMovementOpcode(op);

  MovementInfo const previousPosition = _position;

  if (trustFullPosition) {
    _position = move;
  } else if (mergeMovementState) {
    // Non-heartbeat MSG_MOVE_* share the alternate packed layout; coordinates can be
    // unreliable, but movement flags/time match the client state machine — merge them
    // so server-side queries stay in sync between heartbeats.
    _position.flags = move.flags & kMovementFlagsWireMask;
    _position.flags2 =
        static_cast<uint16>(move.flags2 & kMovementFlags2WireMask);
    _position.time = move.time;
  }

  if (parsed && (trustFullPosition || mergeMovementState))
    ApplyGmFlyAuthority(_position, _gmFlyEnabled, _breathMirrorActive);

  // The game client expects the server to echo MSG_MOVE_* payloads for these opcodes.
  // If parsing fails (wrong layout for a given opcode), still echo the raw bytes so
  // the client state machine does not stall; only apply map/DB position when parsed.
  if (WsIsClientMovementOpcode(op)) {
    bool positionChanged = false;
    if (trustFullPosition) {
      float const dx = _position.x - previousPosition.x;
      float const dy = _position.y - previousPosition.y;
      float const dz = _position.z - previousPosition.z;
      positionChanged = (dx * dx + dy * dy + dz * dz) > 0.0001f;
    }
    TryClearEmotesOnMovement(op, positionChanged);

    auto map = runtime().GetMap(_mapId);
    if (map) {
      if (trustFullPosition)
        map->UpdateObjectPosition(_playerGuid, move);
      else if (mergeMovementState)
        map->UpdateObjectPosition(_playerGuid, _position);
      WorldPacket broadcast(packet.GetOpcode(), packet.Size());
      broadcast.Append(packet.GetBuffer(), packet.Size());
      map->BroadcastPacketToNearby(_playerGuid, broadcast);
      SendPacket(broadcast);
    }
    // `MSG_MOVE_START_SWIM` / `MSG_MOVE_STOP_SWIM` often carry the transition before
    // `MOVEMENTFLAG_SWIMMING` appears on merged heartbeats — mirror breath from opcode too.
    bool inLiquidForBreath = MovementIsSwimming(_position);
    if (op == MSG_MOVE_START_SWIM)
      inLiquidForBreath = true;
    else if (op == MSG_MOVE_STOP_SWIM)
      inLiquidForBreath = false;
    UpdateBreathFromSwimmingState(inLiquidForBreath);
    SyncPlayerMovementHintsIfNeeded(inLiquidForBreath);
  }
}

void WorldSession::HandleForceSpeedChangeAck(WorldPacket &packet) {
  if (_playerGuid == 0)
    return;

  MovementInfo move{};
  if (!TryReadClientMovementMse(packet, packet.GetOpcode(), move)) {
    LOG_DEBUG("[MOVE] Force speed change ack opcode 0x{:04X}: payload not fully decoded "
              "(size={}), discarding",
              packet.GetOpcode(), packet.Size());
    packet.SetReadPos(packet.Size());
    return;
  }

  if (auto map = runtime().GetMap(_mapId)) {
    if (move.x != 0.f || move.y != 0.f || move.z != 0.f)
      map->UpdateObjectPosition(_playerGuid, move);
  }
}

} // namespace Firelands
