#include <gtest/gtest.h>
#include <shared/game/WowGuid.h>
#include <shared/network/WorldPacket.h>
#include <shared/network/packets/client/PackedPlayerGuidWire.h>

using namespace Firelands;

TEST(PackedPlayerGuidWireTests, PlayerLowGuid11RoundTripsTwoByteWire) {
  uint64_t const playerGuid = MakePlayerObjectGuid(11u);
  WorldPacket packet;
  packet.Append<uint8>(0x20); // mask: only g0 (byte index 0) present
  packet.Append<uint8>(0x0C); // 11 ^ 1

  packet.SetReadPos(0);
  uint64_t decoded = 0;
  WorldPackets::Client::ReadLoginPackedPlayerGuid(packet, decoded);
  EXPECT_EQ(decoded, playerGuid);
}

TEST(PackedPlayerGuidWireTests, LegacyPackedGuidMisreadsCataTwoBytePayload) {
  WorldPacket packet;
  packet.Append<uint8>(0x10); // legacy mask: byte index 4 only
  packet.Append<uint8>(0x0A);

  packet.SetReadPos(0);
  EXPECT_EQ(packet.ReadPackedGuid(), 42949672960ULL);
  EXPECT_NE(42949672960ULL, MakePlayerObjectGuid(11u));
}
