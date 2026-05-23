#include <shared/game/WowGuid.h>
#include <shared/network/BitReader.h>
#include <shared/network/packets/server/VendorPackets.h>
#include <shared/network/WorldOpcodes.h>
#include <gtest/gtest.h>

namespace Firelands {
namespace {

/// Mirrors WowPacketParser `V4_3_4_15595` `HandleVendorInventoryList` read order.
void ParseVendorInventoryEmpty(WorldPacket const &pkt, uint64_t &outGuid, uint32_t &outCount,
                               uint8_t &outReason) {
  WorldPacket copy = pkt;
  copy.SetReadPos(0);
  outGuid = 0;

  uint8_t mask[8] = {};
  BitReader br(copy);
  mask[1] = br.ReadBit();
  mask[0] = br.ReadBit();
  outCount = br.ReadBits(21);
  mask[3] = br.ReadBit();
  mask[6] = br.ReadBit();
  mask[5] = br.ReadBit();
  mask[2] = br.ReadBit();
  mask[7] = br.ReadBit();
  mask[4] = br.ReadBit();
  br.AlignToByteBoundary();

  auto readXor = [&](int idx) {
    if (!mask[idx])
      return;
    uint8_t b = copy.Read<uint8_t>() ^ 1u;
    outGuid |= static_cast<uint64_t>(b) << (idx * 8);
  };

  readXor(5);
  readXor(4);
  readXor(1);
  readXor(0);
  readXor(6);
  outReason = copy.Read<uint8_t>();
  readXor(2);
  readXor(3);
  readXor(7);
}

} // namespace

TEST(VendorPacketTests, BuildVendorInventory_EmptyListMatches15595Layout) {
  uint64_t const vendorGuid = MakeCreatureObjectGuid(1234, 0x70000002u);
  WorldPacket pkt = vendor_wire::BuildVendorInventory(vendorGuid, {});

  EXPECT_EQ(pkt.GetOpcode(), static_cast<uint32>(SMSG_VENDOR_INVENTORY));

  uint64_t parsedGuid = 0;
  uint32_t count = 99;
  uint8_t reason = 0;
  ParseVendorInventoryEmpty(pkt, parsedGuid, count, reason);

  EXPECT_EQ(parsedGuid, vendorGuid);
  EXPECT_EQ(count, 0u);
  EXPECT_EQ(reason, 1u);
}

} // namespace Firelands
