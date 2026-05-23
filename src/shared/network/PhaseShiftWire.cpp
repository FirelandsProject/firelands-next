#include <shared/network/PhaseShiftWire.h>

#include <shared/network/WorldOpcodes.h>

namespace Firelands {
namespace PhaseShiftWire {

WorldPacket BuildPhaseShiftChange(uint64 clientGuid, PhaseShift const &phaseShift) {
  WorldPacket pkt(SMSG_PHASE_SHIFT_CHANGE, 64);
  pkt.AppendPackGUID(clientGuid);

  pkt.Append<uint32>(phaseShift.flags);
  pkt.Append<uint32>(static_cast<uint32>(phaseShift.phases.size()));
  for (PhaseRef const &phase : phaseShift.phases) {
    pkt.Append<uint32>(phase.phaseFlags);
    pkt.Append<uint16>(phase.id);
  }
  pkt.AppendPackGUID(phaseShift.personalGuid);

  pkt.Append<uint32>(0u);
  pkt.Append<uint32>(0u);
  pkt.Append<uint32>(0u);
  return pkt;
}

} // namespace PhaseShiftWire
} // namespace Firelands
