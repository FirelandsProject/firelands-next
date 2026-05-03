#pragma once

#include <shared/network/BitWriter.h>
#include <shared/network/MovementSetPackets.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>

namespace Firelands {

namespace movement_teleport_packets_detail {
using movement_set_packets_detail::GuidByteLe;
} // namespace movement_teleport_packets_detail

/// Cataclysm 4.3.4 (15595): `MSG_MOVE_TELEPORT` — `WorldPackets::Movement::MoveTeleport`
/// no vehicle / transport payload.
inline WorldPacket BuildMsgMoveTeleport(uint64 moverGuid, uint32 sequenceIndex, float x,
                                        float y, float z, float facing) {
  using movement_teleport_packets_detail::GuidByteLe;
  auto G = [&](unsigned i) { return GuidByteLe(moverGuid, i); };

  WorldPacket pkt(static_cast<uint32>(MSG_MOVE_TELEPORT), 64);
  BitWriter bw(pkt);
  bw.WriteBitMask(G(6));
  bw.WriteBitMask(G(0));
  bw.WriteBitMask(G(3));
  bw.WriteBitMask(G(2));
  bw.WriteBit(false); // Vehicle.has_value()
  bw.WriteBit(false); // TransportGUID.has_value()
  bw.WriteBitMask(G(1));
  bw.WriteBitMask(G(4));
  bw.WriteBitMask(G(7));
  bw.WriteBitMask(G(5));
  bw.Flush();

  pkt.Append<uint32>(sequenceIndex);
  pkt.WriteByteSeq(G(1));
  pkt.WriteByteSeq(G(2));
  pkt.WriteByteSeq(G(3));
  pkt.WriteByteSeq(G(5));
  pkt.Append<float>(x);
  pkt.WriteByteSeq(G(4));
  pkt.Append<float>(facing);
  pkt.WriteByteSeq(G(7));
  pkt.Append<float>(z);
  pkt.WriteByteSeq(G(0));
  pkt.WriteByteSeq(G(6));
  pkt.Append<float>(y);
  return pkt;
}

/// `SMSG_MOVE_UPDATE_TELEPORT` — standing move info at destination (minimal flags).
inline WorldPacket BuildSmsgMoveUpdateTeleport(uint64 moverGuid, float x, float y, float z,
                                               float orientation, uint32 movementFlags,
                                               uint16 movementFlags2, uint32 timeMs) {
  using movement_teleport_packets_detail::GuidByteLe;
  auto G = [&](unsigned i) { return GuidByteLe(moverGuid, i); };

  bool const hasOrientation = true;
  bool const hasSpline = false;
  bool const hasMovementFlags = movementFlags != 0;
  bool const hasFallData = false;
  bool const hasTransportData = false;
  bool const hasHeightChangeFailed = false;
  bool const hasPitch = false;
  bool const hasExtraMovementFlags = movementFlags2 != 0;
  bool const hasTime = timeMs != 0;
  bool const hasSplineElevation = false;

  WorldPacket pkt(static_cast<uint32>(SMSG_MOVE_UPDATE_TELEPORT), 96);
  pkt.Append<float>(z);
  pkt.Append<float>(y);
  pkt.Append<float>(x);

  BitWriter bw(pkt);
  bw.WriteBit(!hasOrientation);
  bw.WriteBit(hasSpline);
  bw.WriteBit(!hasMovementFlags);
  bw.WriteBitMask(G(2));
  bw.WriteBitMask(G(4));
  bw.WriteBitMask(G(6));
  bw.WriteBit(hasFallData);
  bw.WriteBitMask(G(0));
  bw.WriteBit(hasTransportData);
  bw.WriteBitMask(G(5));

  bw.WriteBit(hasHeightChangeFailed);
  bw.WriteBitMask(G(7));
  bw.WriteBitMask(G(3));

  bw.WriteBit(!hasPitch);
  bw.WriteBit(!hasExtraMovementFlags);
  bw.WriteBit(!hasTime);

  if (hasFallData)
    bw.WriteBit(false); // HasFallDirection

  if (hasExtraMovementFlags)
    bw.WriteBits(movementFlags2, 12);

  bw.WriteBit(!hasSplineElevation);

  if (hasMovementFlags)
    bw.WriteBits(movementFlags, 30);

  bw.WriteBitMask(G(1));
  bw.Flush();

  pkt.WriteByteSeq(G(7));
  pkt.WriteByteSeq(G(6));

  if (hasPitch)
    pkt.Append<float>(0.f);

  if (hasSplineElevation)
    pkt.Append<float>(0.f);

  if (hasOrientation)
    pkt.Append<float>(orientation);

  pkt.WriteByteSeq(G(2));
  pkt.WriteByteSeq(G(3));
  pkt.WriteByteSeq(G(1));

  pkt.WriteByteSeq(G(5));
  pkt.WriteByteSeq(G(4));

  if (hasTime)
    pkt.Append<uint32>(timeMs);

  pkt.WriteByteSeq(G(0));
  return pkt;
}

} // namespace Firelands
