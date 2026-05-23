#pragma once

#include <shared/game/PhaseShift.h>
#include <shared/network/WorldPacket.h>

namespace Firelands {
namespace PhaseShiftWire {

/// `SMSG_PHASE_SHIFT_CHANGE` (4.3.4.15595) — aligns with TrinityCore `PhaseShiftChange`.
WorldPacket BuildPhaseShiftChange(uint64 clientGuid, PhaseShift const &phaseShift);

} // namespace PhaseShiftWire
} // namespace Firelands
