#pragma once

#include <optional>
#include <shared/network/BitWriter.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <cstdint>
#include <vector>

namespace Firelands::vendor_wire {

/// Cataclysm 4.3.4 vendor list item (`SMSG_VENDOR_INVENTORY` / WowPacketParser 15595).
struct VendorWireItem {
  int32_t muId = 0;
  int32_t durability = 0;
  int32_t itemId = 0;
  int32_t type = 1;
  int32_t price = 0;
  int32_t itemDisplayInfoId = 0;
  int32_t quantity = -1;
  int32_t stackCount = 1;
  std::optional<int32_t> extendedCostId;
  std::optional<int32_t> playerConditionFailed;
};

inline void GuidBytesFromUint64(uint64_t guid, uint8_t out[8]) {
  for (int i = 0; i < 8; ++i)
    out[i] = static_cast<uint8_t>((guid >> (i * 8)) & 0xFF);
}

/// WowPacketParser `V4_3_4_15595` — `SMSG_VENDOR_INVENTORY` (0x7CB0).
inline WorldPacket BuildVendorInventory(uint64_t vendorGuid,
                                        std::vector<VendorWireItem> const &items,
                                        uint8_t reasonWhenEmpty = 1) {
  uint8_t G[8] = {};
  GuidBytesFromUint64(vendorGuid, G);

  WorldPacket pkt(SMSG_VENDOR_INVENTORY, 64);
  BitWriter bw(pkt);

  bw.WriteBitMask(G[1]);
  bw.WriteBitMask(G[0]);
  bw.WriteBits(static_cast<uint32>(items.size()), 21);
  bw.WriteBitMask(G[3]);
  bw.WriteBitMask(G[6]);
  bw.WriteBitMask(G[5]);
  bw.WriteBitMask(G[2]);
  bw.WriteBitMask(G[7]);

  for (VendorWireItem const &item : items) {
    bw.WriteBit(!item.extendedCostId.has_value());
    bw.WriteBit(!item.playerConditionFailed.has_value());
  }

  bw.WriteBitMask(G[4]);
  bw.Flush();

  for (VendorWireItem const &item : items) {
    pkt.Append<int32_t>(item.muId);
    pkt.Append<int32_t>(item.durability);
    if (item.extendedCostId)
      pkt.Append<int32_t>(*item.extendedCostId);
    pkt.Append<int32_t>(item.itemId);
    pkt.Append<int32_t>(item.type);
    pkt.Append<int32_t>(item.price);
    pkt.Append<int32_t>(item.itemDisplayInfoId);
    if (item.playerConditionFailed)
      pkt.Append<int32_t>(*item.playerConditionFailed);
    pkt.Append<int32_t>(item.quantity);
    pkt.Append<int32_t>(item.stackCount);
  }

  pkt.WriteByteSeq(G[5]);
  pkt.WriteByteSeq(G[4]);
  pkt.WriteByteSeq(G[1]);
  pkt.WriteByteSeq(G[0]);
  pkt.WriteByteSeq(G[6]);
  pkt.Append<uint8_t>(items.empty() ? reasonWhenEmpty : 0u);
  pkt.WriteByteSeq(G[2]);
  pkt.WriteByteSeq(G[3]);
  pkt.WriteByteSeq(G[7]);

  return pkt;
}

} // namespace Firelands::vendor_wire
