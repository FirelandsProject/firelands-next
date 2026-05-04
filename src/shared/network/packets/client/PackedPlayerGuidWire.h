#pragma once

#include <cstdint>
#include <cstring>
#include <shared/network/BitReader.h>
#include <shared/network/WorldPacket.h>

namespace Firelands::WorldPackets::Client {

/// Cataclysm 4.3.4: bit-packed player `ObjectGuid` used at the start of
/// `CMSG_PLAYER_LOGIN` and `CMSG_SET_SELECTION` (mask + masked bytes `^ 1`).
inline void ReadLoginPackedPlayerGuid(WorldPacket &packet, uint64_t &outGuid) {
  uint8_t guidBytes[8] = {0};
  BitReader br(packet);

  bool const g2 = br.ReadBit();
  bool const g3 = br.ReadBit();
  bool const g0 = br.ReadBit();
  bool const g6 = br.ReadBit();
  bool const g4 = br.ReadBit();
  bool const g5 = br.ReadBit();
  bool const g1 = br.ReadBit();
  bool const g7 = br.ReadBit();

  if (g2)
    guidBytes[2] = packet.Read<uint8>() ^ 1;
  if (g7)
    guidBytes[7] = packet.Read<uint8>() ^ 1;
  if (g0)
    guidBytes[0] = packet.Read<uint8>() ^ 1;
  if (g3)
    guidBytes[3] = packet.Read<uint8>() ^ 1;
  if (g5)
    guidBytes[5] = packet.Read<uint8>() ^ 1;
  if (g6)
    guidBytes[6] = packet.Read<uint8>() ^ 1;
  if (g1)
    guidBytes[1] = packet.Read<uint8>() ^ 1;
  if (g4)
    guidBytes[4] = packet.Read<uint8>() ^ 1;

  outGuid = 0;
  std::memcpy(&outGuid, guidBytes, 8);
}

} // namespace Firelands::WorldPackets::Client
