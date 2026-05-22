#include <gtest/gtest.h>
#include <shared/game/ActionBarToggles.h>
#include <shared/network/UpdateFields.h>

#include <cstring>
#include <map>

using namespace Firelands;
using namespace Firelands::ActionBarToggles;

namespace {

void SetPlayerFieldByte(std::map<uint16, uint32> &fields, uint16 field, uint8 offset,
                        uint8 value) {
  uint32 &packed = fields[field];
  uint8 bytes[4]{};
  std::memcpy(bytes, &packed, sizeof(bytes));
  if (offset < 4)
    bytes[offset] = value;
  std::memcpy(&packed, bytes, sizeof(bytes));
}

} // namespace

TEST(ActionBarTogglesTests, ExtraBarsVisibleMaskIs0x0F) {
  EXPECT_EQ(kAllExtraBars, 0x0Fu);
}

TEST(ActionBarTogglesTests, PlayerFieldBytesStoresTogglesAtByteOffset2) {
  std::map<uint16, uint32> fields;
  SetPlayerFieldByte(fields, PLAYER_FIELD_BYTES,
                     PLAYER_FIELD_BYTES_OFFSET_ACTION_BAR_TOGGLES, kAllExtraBars);
  EXPECT_EQ(fields[PLAYER_FIELD_BYTES], 0x000F0000u);

  SetPlayerFieldByte(fields, PLAYER_FIELD_BYTES,
                     PLAYER_FIELD_BYTES_OFFSET_ACTION_BAR_TOGGLES, kDefaultVisible);
  EXPECT_EQ(fields[PLAYER_FIELD_BYTES], 0x00FF0000u);

  SetPlayerFieldByte(fields, PLAYER_FIELD_BYTES,
                     PLAYER_FIELD_BYTES_OFFSET_ACTION_BAR_TOGGLES, kAllHidden);
  EXPECT_EQ(fields[PLAYER_FIELD_BYTES], 0u);
}
