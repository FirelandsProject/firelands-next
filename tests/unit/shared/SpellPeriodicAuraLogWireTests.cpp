#include <gtest/gtest.h>
#include <shared/game/SpellAuraTypes.h>
#include <shared/network/SpellPeriodicAuraLogWire.h>
#include <shared/network/WorldOpcodes.h>

using namespace Firelands;

TEST(SpellPeriodicAuraLogWireTests, BuildPeriodicHealMatchesreferenceLayout) {
  WorldPacket pkt;
  SpellPeriodicAuraLogWire::BuildPeriodicHeal(pkt, 0x50ULL, 0x100ULL, 774u, 42u);

  EXPECT_EQ(pkt.GetOpcode(), SMSG_PERIODICAURALOG);
  pkt.SetReadPos(0);
  EXPECT_EQ(pkt.ReadPackedGuid(), 0x50ULL);
  EXPECT_EQ(pkt.ReadPackedGuid(), 0x100ULL);
  EXPECT_EQ(pkt.Read<uint32>(), 774u);
  EXPECT_EQ(pkt.Read<uint32>(), 1u);
  EXPECT_EQ(pkt.Read<uint32>(), kSpellAuraPeriodicHeal);
  EXPECT_EQ(pkt.Read<uint32>(), 42u);
  EXPECT_EQ(pkt.Read<uint32>(), 0u);
  EXPECT_EQ(pkt.Read<uint32>(), 0u);
  EXPECT_EQ(pkt.Read<uint8>(), 0u);
  EXPECT_EQ(pkt.GetReadPos(), pkt.Size());
}
