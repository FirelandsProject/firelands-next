#include <gtest/gtest.h>
#include <shared/game/WowGuid.h>
#include <shared/network/packets/server/CombatPackets.h>

using namespace Firelands;

namespace {

uint64_t TestPlayerGuid() { return MakePlayerObjectGuid(1); }

uint64_t TestCreatureGuid() { return MakeCreatureObjectGuid(100, 1); }

} // namespace

TEST(CombatPacketsTest, AttackStopEndsWithSingleNowDeadBit) {
  WorldPacket const pkt =
      combat_wire::BuildAttackStop(TestPlayerGuid(), TestCreatureGuid(), true);
  ASSERT_GE(pkt.Size(), 3u);
  uint8_t const tail = pkt.GetBuffer()[pkt.Size() - 1];
  EXPECT_NE(tail & 0x80u, 0u);
}

TEST(CombatPacketsTest, AttackerStateUpdateMatchesCataclysmLayout) {
  WorldPacket pkt = combat_wire::BuildAttackerStateUpdate(
      TestPlayerGuid(), TestCreatureGuid(), 50u, 30u);
  EXPECT_GE(pkt.Size(), 24u);
  EXPECT_EQ(pkt.GetOpcode(), static_cast<uint32>(SMSG_ATTACKERSTATE_UPDATE));
  EXPECT_EQ(pkt.Read<int32_t>(), 0x00000001);
}

TEST(CombatPacketsTest, SpellNonMeleeDamageLogUsesExpectedOpcode) {
  WorldPacket pkt = combat_wire::BuildSpellNonMeleeDamageLog(
      TestCreatureGuid(), TestPlayerGuid(), 133u, 42u, 10u);
  EXPECT_EQ(pkt.GetOpcode(), static_cast<uint32>(SMSG_SPELLNONMELEEDAMAGELOG));
  EXPECT_GE(pkt.Size(), 20u);
}

TEST(CombatPacketsTest, PowerUpdateMatchesCataclysmRefLayout) {
  WorldPacket pkt = combat_wire::BuildPowerUpdate(TestPlayerGuid(), 0u, 900);
  EXPECT_EQ(pkt.GetOpcode(), static_cast<uint32>(SMSG_POWER_UPDATE));
  EXPECT_GE(pkt.Size(), 12u);
  EXPECT_EQ(pkt.ReadPackedGuid(), TestPlayerGuid());
  EXPECT_EQ(pkt.Read<uint32_t>(), 1u);
  EXPECT_EQ(pkt.Read<uint8_t>(), 0u);
  EXPECT_EQ(pkt.Read<int32_t>(), 900);
}
