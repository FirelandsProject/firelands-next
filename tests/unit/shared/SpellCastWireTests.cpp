#include <gtest/gtest.h>
#include <shared/network/SpellCastWire.h>
#include <shared/network/WorldOpcodes.h>

using namespace Firelands;

TEST(SpellCastWireTests, TryReadClientCastSpell_MinimalPayload) {
  WorldPacket p(CMSG_CAST_SPELL, 64);
  p.Append<uint8>(3);   // cast id
  p.Append<int32>(6673); // Battle Shout
  p.Append<int32>(0);  // misc
  p.Append<uint8>(0);  // send cast flags
  p.Append<uint32>(0); // target flags

  SpellCastWire::ClientCastSpellData c;
  ASSERT_TRUE(SpellCastWire::TryReadClientCastSpell(p, c));
  EXPECT_EQ(c.castId, 3);
  EXPECT_EQ(c.spellId, 6673);
  EXPECT_EQ(c.misc, 0);
  EXPECT_EQ(c.sendCastFlags, 0);
  EXPECT_EQ(c.targetFlags, 0u);
  EXPECT_EQ(c.unitTargetGuid, 0u);
}

TEST(SpellCastWireTests, BuildSpellStartAndGo_RoundtripSize) {
  uint64 const guid = 0x0000000000ABCDEFULL;
  WorldPacket start;
  SpellCastWire::BuildSpellStart(start, guid, 1, 6673,
                                 SpellCastWire::CAST_FLAG_HAS_TRAJECTORY, 0, 0,
                                 SpellCastWire::TARGET_FLAG_UNIT, guid);
  EXPECT_EQ(start.GetOpcode(), SMSG_SPELL_START);
  EXPECT_GT(start.Size(), 16u);

  WorldPacket go;
  std::vector<uint64> hits = {guid};
  auto const nowMs = uint32_t{12345678};
  SpellCastWire::BuildSpellGo(go, guid, 1, 6673,
                              SpellCastWire::CAST_FLAG_UNKNOWN_9, 0, nowMs,
                              hits, SpellCastWire::TARGET_FLAG_UNIT, guid);
  EXPECT_EQ(go.GetOpcode(), SMSG_SPELL_GO);
  EXPECT_GT(go.Size(), start.Size());
}
