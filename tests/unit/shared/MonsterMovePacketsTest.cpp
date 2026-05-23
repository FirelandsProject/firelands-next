#include <gtest/gtest.h>
#include <shared/game/WowGuid.h>
#include <shared/network/MonsterMovePackets.h>
#include <shared/network/WorldOpcodes.h>

using namespace Firelands;

TEST(MonsterMovePacketsTest, ChasePacketUsesOnMonsterMoveOpcode) {
  WorldPacket const pkt = monster_move_wire::BuildMonsterMoveToPosition(
      MakeCreatureObjectGuid(1, 1), 0.f, 0.f, 0.f, 5.f, 0.f, 0.f, 1, 500u,
      monster_move_wire::FacingAngle, 0.f);
  EXPECT_EQ(pkt.GetOpcode(), static_cast<uint32>(SMSG_ON_MONSTER_MOVE));
  EXPECT_GE(pkt.Size(), 40u);
}

TEST(MonsterMovePacketsTest, StopPacketEndsAfterStopFace) {
  WorldPacket pkt =
      monster_move_wire::BuildMonsterMoveStop(MakeCreatureObjectGuid(1, 1), 1.f, 2.f, 3.f, 9);
  EXPECT_EQ(pkt.GetOpcode(), static_cast<uint32>(SMSG_ON_MONSTER_MOVE));
  pkt.SetReadPos(0);
  (void)pkt.ReadPackedGuid();
  (void)pkt.Read<int8_t>();
  (void)pkt.Read<float>();
  (void)pkt.Read<float>();
  (void)pkt.Read<float>();
  EXPECT_EQ(pkt.Read<int32_t>(), 9);
  EXPECT_EQ(static_cast<int8_t>(pkt.Read<int8_t>()),
            static_cast<int8_t>(monster_move_wire::Stop));
  EXPECT_EQ(pkt.GetReadPos(), pkt.Size());
}

TEST(MonsterMovePacketsTest, FacingTargetEmbedsRawUint64Guid) {
  uint64_t const playerGuid = MakePlayerObjectGuid(1);
  WorldPacket pkt = monster_move_wire::BuildMonsterMoveToPosition(
      MakeCreatureObjectGuid(1, 1), 0.f, 0.f, 0.f, 5.f, 0.f, 0.f, 1, 500u,
      monster_move_wire::FacingTarget, 0.f, playerGuid);
  pkt.SetReadPos(0);
  (void)pkt.ReadPackedGuid();
  (void)pkt.Read<int8_t>();
  (void)pkt.Read<float>();
  (void)pkt.Read<float>();
  (void)pkt.Read<float>();
  (void)pkt.Read<int32_t>();
  EXPECT_EQ(static_cast<int8_t>(pkt.Read<int8_t>()),
            static_cast<int8_t>(monster_move_wire::FacingTarget));
  EXPECT_EQ(pkt.Read<uint64_t>(), playerGuid);
}

TEST(MonsterMovePacketsTest, DurationScalesWithPathLength) {
  uint32_t const shortMs = monster_move_wire::MonsterMoveDurationMs(
      0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 7.f);
  uint32_t const longMs = monster_move_wire::MonsterMoveDurationMs(
      0.f, 0.f, 0.f, 10.f, 0.f, 0.f, 7.f);
  EXPECT_GE(shortMs, 200u);
  EXPECT_GT(longMs, shortMs);
}
