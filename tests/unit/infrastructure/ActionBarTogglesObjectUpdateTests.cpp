#include <gtest/gtest.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/game/ActionBarToggles.h>
#include <shared/network/UpdateFields.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>

using namespace Firelands;

namespace ws_obj = WorldSessionObjectUpdate;

TEST(ActionBarTogglesObjectUpdateTests, ValuesUpdateEncodesToggleByteInPlayerFieldBytes) {
  WorldPacket out;
  ws_obj::BuildPlayerActionBarTogglesValuesUpdate(
      1, 0x11, ActionBarToggles::kAllExtraBars, out);

  ASSERT_GT(out.Size(), 0u);
  EXPECT_EQ(out.GetOpcode(), static_cast<uint32>(SMSG_UPDATE_OBJECT));

  out.SetReadPos(0);
  EXPECT_EQ(out.Read<uint16>(), 1u);
  ASSERT_EQ(out.Read<uint32>(), 1u);
  EXPECT_EQ(out.Read<uint8>(), static_cast<uint8>(UPDATETYPE_VALUES));

  uint64 const readGuid = out.ReadPackedGuid();
  EXPECT_EQ(readGuid, 0x11u);

  uint8 maskSize = out.Read<uint8>();
  ASSERT_GE(maskSize, 1u);
  std::vector<uint32> blockMask(maskSize);
  for (uint8 i = 0; i < maskSize; ++i)
    blockMask[i] = out.Read<uint32>();

  uint16 const fieldIndex = PLAYER_FIELD_BYTES;
  ASSERT_LT(fieldIndex / 32, blockMask.size());
  ASSERT_NE(blockMask[fieldIndex / 32] & (1u << (fieldIndex % 32)), 0u);

  uint32 const packed = out.Read<uint32>();
  EXPECT_EQ(packed, 0x000F0000u);
}
