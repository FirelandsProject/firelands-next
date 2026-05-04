#pragma once

#include <shared/network/MovementInfo.h>
#include <shared/network/WorldPacket.h>

namespace Firelands {

/// Decode client `MSG_MOVE_*` payloads using the opcode-specific Movement Status
/// Element (MSE) sequence used on the wire for this project’s client build.
bool TryReadClientMovementMse(WorldPacket &packet, uint32 opcode, MovementInfo &move);

} // namespace Firelands
