#include <shared/network/MovementWire.h>
#include <shared/network/movement/ClientMovementMse.h>

namespace Firelands {

bool TryReadClientMovement(WorldPacket &packet, WorldOpcode opcode,
                           MovementInfo &move) {
  return TryReadClientMovementMse(packet, static_cast<uint32>(opcode), move);
}

} // namespace Firelands
