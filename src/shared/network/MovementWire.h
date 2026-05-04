#pragma once

#include <shared/network/MovementInfo.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>

namespace Firelands {

/// Decode client movement payloads into \a move for the wire layout the server
/// currently implements (see \c MovementWire.cpp). When the client layout changes,
/// extend the decoder in `ClientMovementMse` / opcode sequences rather than
/// renaming this entry point.
bool TryReadClientMovement(WorldPacket &packet, WorldOpcode opcode,
                           MovementInfo &move);

} // namespace Firelands
