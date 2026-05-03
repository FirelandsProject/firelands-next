#pragma once

#include <shared/network/BitWriter.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>

namespace Firelands {

namespace movement_set_packets_detail {

inline uint8 GuidByteLe(uint64 guid, unsigned index) {
  return static_cast<uint8>((guid >> (index * 8)) & 0xFFu);
}

} // namespace movement_set_packets_detail

/// Cataclysm 4.3.4 (15595): `SMSG_MOVE_SET_RUN_SPEED` uses the scrambled movement
/// layout (`MoveSetRunSpeed[]` in MovementStructures.cpp), not
/// `packed guid + counter + float`.
inline WorldPacket BuildSmsgMoveSetRunSpeed(uint64 moverGuid, uint32 moveCounter,
                                            float speed) {
  using movement_set_packets_detail::GuidByteLe;
  WorldPacket pkt(static_cast<uint32>(SMSG_MOVE_SET_RUN_SPEED), 48);
  auto G = [&](unsigned i) { return GuidByteLe(moverGuid, i); };
  BitWriter bw(pkt);
  bw.WriteBitMask(G(6));
  bw.WriteBitMask(G(1));
  bw.WriteBitMask(G(5));
  bw.WriteBitMask(G(2));
  bw.WriteBitMask(G(7));
  bw.WriteBitMask(G(0));
  bw.WriteBitMask(G(3));
  bw.WriteBitMask(G(4));
  bw.Flush();
  pkt.WriteByteSeq(G(5));
  pkt.WriteByteSeq(G(3));
  pkt.WriteByteSeq(G(1));
  pkt.WriteByteSeq(G(4));
  pkt.Append<uint32>(moveCounter);
  pkt.Append<float>(speed);
  pkt.WriteByteSeq(G(6));
  pkt.WriteByteSeq(G(0));
  pkt.WriteByteSeq(G(7));
  pkt.WriteByteSeq(G(2));
  return pkt;
}

/// `SMSG_MOVE_SET_FLIGHT_SPEED` — `MoveSetFlightSpeed[]` layout (MovementStructures.cpp).
inline WorldPacket BuildSmsgMoveSetFlightSpeed(uint64 moverGuid, uint32 moveCounter,
                                               float speed) {
  using movement_set_packets_detail::GuidByteLe;
  WorldPacket pkt(static_cast<uint32>(SMSG_MOVE_SET_FLIGHT_SPEED), 48);
  auto G = [&](unsigned i) { return GuidByteLe(moverGuid, i); };
  BitWriter bw(pkt);
  bw.WriteBitMask(G(0));
  bw.WriteBitMask(G(5));
  bw.WriteBitMask(G(1));
  bw.WriteBitMask(G(6));
  bw.WriteBitMask(G(3));
  bw.WriteBitMask(G(2));
  bw.WriteBitMask(G(7));
  bw.WriteBitMask(G(4));
  bw.Flush();
  pkt.WriteByteSeq(G(0));
  pkt.WriteByteSeq(G(1));
  pkt.WriteByteSeq(G(7));
  pkt.WriteByteSeq(G(5));
  pkt.Append<float>(speed);
  pkt.Append<uint32>(moveCounter);
  pkt.WriteByteSeq(G(2));
  pkt.WriteByteSeq(G(6));
  pkt.WriteByteSeq(G(3));
  pkt.WriteByteSeq(G(4));
  return pkt;
}

/// `SMSG_MOVE_SET_CAN_FLY` — `MoveSetCanFly[]` layout (no extra float).
inline WorldPacket BuildSmsgMoveSetCanFly(uint64 moverGuid, uint32 moveCounter) {
  using movement_set_packets_detail::GuidByteLe;
  WorldPacket pkt(static_cast<uint32>(SMSG_MOVE_SET_CAN_FLY), 40);
  auto G = [&](unsigned i) { return GuidByteLe(moverGuid, i); };
  BitWriter bw(pkt);
  bw.WriteBitMask(G(1));
  bw.WriteBitMask(G(6));
  bw.WriteBitMask(G(5));
  bw.WriteBitMask(G(0));
  bw.WriteBitMask(G(7));
  bw.WriteBitMask(G(4));
  bw.WriteBitMask(G(2));
  bw.WriteBitMask(G(3));
  bw.Flush();
  pkt.WriteByteSeq(G(6));
  pkt.WriteByteSeq(G(3));
  pkt.Append<uint32>(moveCounter);
  pkt.WriteByteSeq(G(2));
  pkt.WriteByteSeq(G(1));
  pkt.WriteByteSeq(G(4));
  pkt.WriteByteSeq(G(7));
  pkt.WriteByteSeq(G(0));
  pkt.WriteByteSeq(G(5));
  return pkt;
}

/// `SMSG_MOVE_UNSET_CAN_FLY` — `MoveUnsetCanFly[]` layout.
inline WorldPacket BuildSmsgMoveUnsetCanFly(uint64 moverGuid, uint32 moveCounter) {
  using movement_set_packets_detail::GuidByteLe;
  WorldPacket pkt(static_cast<uint32>(SMSG_MOVE_UNSET_CAN_FLY), 40);
  auto G = [&](unsigned i) { return GuidByteLe(moverGuid, i); };
  BitWriter bw(pkt);
  bw.WriteBitMask(G(1));
  bw.WriteBitMask(G(4));
  bw.WriteBitMask(G(2));
  bw.WriteBitMask(G(5));
  bw.WriteBitMask(G(0));
  bw.WriteBitMask(G(3));
  bw.WriteBitMask(G(6));
  bw.WriteBitMask(G(7));
  bw.Flush();
  pkt.WriteByteSeq(G(4));
  pkt.WriteByteSeq(G(6));
  pkt.Append<uint32>(moveCounter);
  pkt.WriteByteSeq(G(1));
  pkt.WriteByteSeq(G(0));
  pkt.WriteByteSeq(G(2));
  pkt.WriteByteSeq(G(3));
  pkt.WriteByteSeq(G(5));
  pkt.WriteByteSeq(G(7));
  return pkt;
}

} // namespace Firelands
