#include <gtest/gtest.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/game/UnitFieldFlags.h>
#include <shared/network/UpdateFields.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>

using namespace Firelands;

namespace ws_obj = WorldSessionObjectUpdate;

TEST(UnitObjectUpdateTests, UnitHealthValuesUpdateEncodesHealthFields) {
  WorldPacket out;
  ws_obj::BuildUnitHealthValuesUpdate(1, 0x99, 42u, 100u, out);

  ASSERT_GT(out.Size(), 0u);
  EXPECT_EQ(out.GetOpcode(), static_cast<uint32>(SMSG_UPDATE_OBJECT));

  out.SetReadPos(0);
  EXPECT_EQ(out.Read<uint16>(), 1u);
  ASSERT_EQ(out.Read<uint32>(), 1u);
  EXPECT_EQ(out.Read<uint8>(), static_cast<uint8>(UPDATETYPE_VALUES));
  EXPECT_EQ(out.ReadPackedGuid(), 0x99u);

  uint8 const maskSize = out.Read<uint8>();
  ASSERT_GE(maskSize, 2u);
  std::vector<uint32> blockMask(maskSize);
  for (uint8 i = 0; i < maskSize; ++i)
    blockMask[i] = out.Read<uint32>();

  ASSERT_NE(blockMask[UNIT_FIELD_HEALTH / 32] & (1u << (UNIT_FIELD_HEALTH % 32)), 0u);
  ASSERT_NE(blockMask[UNIT_FIELD_MAXHEALTH / 32] & (1u << (UNIT_FIELD_MAXHEALTH % 32)), 0u);

  for (uint32 i = 0; i < UNIT_FIELD_HEALTH; ++i) {
    if (blockMask[i / 32] & (1u << (i % 32)))
      (void)out.Read<uint32>();
  }
  EXPECT_EQ(out.Read<uint32>(), 42u);
  EXPECT_EQ(out.Read<uint32>(), 100u);
}

TEST(UnitObjectUpdateTests, UnitFlagsValuesUpdateEncodesCombatFlag) {
  WorldPacket out;
  ws_obj::BuildUnitFlagsValuesUpdate(1, 0x55, kUnitFlagInCombat | kUnitFlagCanSwim, out);

  out.SetReadPos(0);
  (void)out.Read<uint16>();
  (void)out.Read<uint32>();
  (void)out.Read<uint8>();
  (void)out.ReadPackedGuid();

  uint8 const maskSize = out.Read<uint8>();
  std::vector<uint32> blockMask(maskSize);
  for (uint8 i = 0; i < maskSize; ++i)
    blockMask[i] = out.Read<uint32>();

  ASSERT_NE(blockMask[UNIT_FIELD_FLAGS / 32] & (1u << (UNIT_FIELD_FLAGS % 32)), 0u);
  for (uint32 i = 0; i < UNIT_FIELD_FLAGS; ++i) {
    if (blockMask[i / 32] & (1u << (i % 32)))
      (void)out.Read<uint32>();
  }
  EXPECT_EQ(out.Read<uint32>(), kUnitFlagInCombat | kUnitFlagCanSwim);
}

TEST(UnitObjectUpdateTests, UnitDynamicFlagsValuesUpdateEncodesLootable) {
  WorldPacket out;
  ws_obj::BuildUnitDynamicFlagsValuesUpdate(
      1, 0x66, kUnitDynflagLootable | kUnitDynflagTappedByPlayer, out);

  out.SetReadPos(0);
  (void)out.Read<uint16>();
  (void)out.Read<uint32>();
  (void)out.Read<uint8>();
  (void)out.ReadPackedGuid();

  uint8 const maskSize = out.Read<uint8>();
  std::vector<uint32> blockMask(maskSize);
  for (uint8 i = 0; i < maskSize; ++i)
    blockMask[i] = out.Read<uint32>();

  ASSERT_NE(blockMask[UNIT_DYNAMIC_FLAGS / 32] & (1u << (UNIT_DYNAMIC_FLAGS % 32)), 0u);
  for (uint32 i = 0; i < UNIT_DYNAMIC_FLAGS; ++i) {
    if (blockMask[i / 32] & (1u << (i % 32)))
      (void)out.Read<uint32>();
  }
  EXPECT_EQ(out.Read<uint32>(), kUnitDynflagLootable | kUnitDynflagTappedByPlayer);
}
