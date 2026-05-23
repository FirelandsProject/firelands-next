#include <gtest/gtest.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/network/UpdateFields.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>

using namespace Firelands;

namespace ws_obj = WorldSessionObjectUpdate;

TEST(Power1ObjectUpdateTests, ValuesUpdateEncodesPower1AndMaxPower1) {
  WorldPacket out;
  ws_obj::BuildPlayerPower1ValuesUpdate(1, 0x42, 120u, 210u, out);

  ASSERT_GT(out.Size(), 0u);
  EXPECT_EQ(out.GetOpcode(), static_cast<uint32>(SMSG_UPDATE_OBJECT));

  out.SetReadPos(0);
  EXPECT_EQ(out.Read<uint16>(), 1u);
  ASSERT_EQ(out.Read<uint32>(), 1u);
  EXPECT_EQ(out.Read<uint8>(), static_cast<uint8>(UPDATETYPE_VALUES));
  EXPECT_EQ(out.ReadPackedGuid(), 0x42u);

  uint8 const maskSize = out.Read<uint8>();
  ASSERT_GE(maskSize, 2u);
  std::vector<uint32> blockMask(maskSize);
  for (uint8 i = 0; i < maskSize; ++i)
    blockMask[i] = out.Read<uint32>();

  ASSERT_NE(blockMask[UNIT_FIELD_POWER1 / 32] & (1u << (UNIT_FIELD_POWER1 % 32)), 0u);
  ASSERT_NE(blockMask[UNIT_FIELD_MAXPOWER1 / 32] & (1u << (UNIT_FIELD_MAXPOWER1 % 32)), 0u);

  for (uint32 i = 0; i < UNIT_FIELD_POWER1; ++i) {
    if (blockMask[i / 32] & (1u << (i % 32)))
      (void)out.Read<uint32>();
  }
  EXPECT_EQ(out.Read<uint32>(), 120u);
  EXPECT_EQ(out.Read<uint32>(), 210u);
}
