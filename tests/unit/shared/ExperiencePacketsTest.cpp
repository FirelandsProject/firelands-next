#include <gtest/gtest.h>
#include <shared/game/WowGuid.h>
#include <shared/network/packets/server/ExperiencePackets.h>

using namespace Firelands;

namespace {

uint64_t TestCreatureGuid() { return MakeCreatureObjectGuid(6, 42); }

} // namespace

TEST(ExperiencePacketsTest, LogXpGainKillMatchesCataclysmLayout) {
  WorldPacket pkt =
      experience_wire::BuildLogXpGain(TestCreatureGuid(), 120, 120);
  EXPECT_EQ(pkt.GetOpcode(), static_cast<uint32>(SMSG_LOG_XP_GAIN));
  EXPECT_EQ(pkt.Read<uint64_t>(), TestCreatureGuid());
  EXPECT_EQ(pkt.Read<int32_t>(), 120);
  EXPECT_EQ(pkt.Read<uint8_t>(), 0u);
  EXPECT_EQ(pkt.Read<int32_t>(), 120);
  EXPECT_FLOAT_EQ(pkt.Read<float>(), 1.0f);
  EXPECT_EQ(pkt.Read<uint8_t>(), 0u);
}

TEST(ExperiencePacketsTest, LogXpGainNoKillOmitsKillFields) {
  WorldPacket pkt = experience_wire::BuildLogXpGain(0, 500, 0, 1.0f,
                                                    experience_wire::LogXpReason::NoKill);
  EXPECT_EQ(pkt.Read<uint64_t>(), 0u);
  EXPECT_EQ(pkt.Read<int32_t>(), 500);
  EXPECT_EQ(pkt.Read<uint8_t>(), 1u);
  EXPECT_EQ(pkt.Read<uint8_t>(), 0u);
}
