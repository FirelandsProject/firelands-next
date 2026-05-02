#include <application/services/WorldService.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionMovementChecks.h>
#include <shared/Logger.h>
#include <shared/network/MovementWire.h>

namespace Firelands {

void WorldSession::HandleMovement(WorldPacket &packet) {
  WorldOpcode const op = static_cast<WorldOpcode>(packet.GetOpcode());
  MovementInfo move{};
  bool const parsed = TryReadClientMovement(packet, op, move);

  // After logout the client may still send movement while transitioning to character
  // select. Echoing those packets breaks that transition (stuck loading).
  if (_playerGuid == 0)
    return;

  bool const canPersistPosition =
      parsed && WsIsTrustedPositionOpcode(op) && WsIsSaneWorldPosition(move);

  if (canPersistPosition)
    _position = move;

  // Cataclysm expects the server to echo MSG_MOVE_* payloads for these opcodes.
  // If parsing fails (wrong layout for a given opcode), still echo the raw bytes so
  // the client state machine does not stall; only apply map/DB position when parsed.
  if (WsIsClientMovementOpcode(op)) {
    auto map = WorldService::Instance().GetMap(_mapId);
    if (map) {
      if (canPersistPosition)
        map->UpdateObjectPosition(_playerGuid, move);
      WorldPacket broadcast(packet.GetOpcode(), packet.Size());
      broadcast.Append(packet.GetBuffer(), packet.Size());
      map->BroadcastPacketToNearby(_playerGuid, broadcast);
      SendPacket(broadcast);
    }
  }
}

} // namespace Firelands
