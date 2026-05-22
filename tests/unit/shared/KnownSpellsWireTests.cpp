#include <gtest/gtest.h>
#include <shared/network/KnownSpellsWire.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>

using namespace Firelands;

TEST(KnownSpellsWireTests, MatchesBuild15595LegacyLayout) {
  std::vector<uint32_t> const spells{133u, 20572u};
  WorldPacket packet(SMSG_SEND_KNOWN_SPELLS);
  KnownSpellsWire::WriteSendKnownSpells(packet, true, spells);

  EXPECT_EQ(packet.Size(), 1u + 2u + spells.size() * (4u + 2u) + 2u);

  packet.SetReadPos(0);
  EXPECT_EQ(packet.Read<uint8_t>(), 1u);
  EXPECT_EQ(packet.Read<uint16_t>(), 2u);
  EXPECT_EQ(packet.Read<uint32_t>(), 133u);
  EXPECT_EQ(packet.Read<int16_t>(), 0);
  EXPECT_EQ(packet.Read<uint32_t>(), 20572u);
  EXPECT_EQ(packet.Read<int16_t>(), 0);
  EXPECT_EQ(packet.Read<uint16_t>(), 0u);
}
