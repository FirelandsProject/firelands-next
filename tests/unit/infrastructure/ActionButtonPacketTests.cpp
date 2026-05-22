#include <gtest/gtest.h>
#include <shared/game/ActionButton.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>

using namespace Firelands;

TEST(ActionButtonPacketTests, InitialActionButtonsPayloadIs144Uint32SlotsPlusReason) {
  ActionButton::PackedActionBar bar{};
  bar[0] = ActionButton::PackWire(133u, ActionButton::Spell);

  WorldPacket data(SMSG_ACTION_BUTTONS);
  data.Append(reinterpret_cast<uint8 const *>(bar.data()),
              ActionButton::kMaxButtons * sizeof(uint32_t));
  data.Append<uint8>(0);

  EXPECT_EQ(data.Size(), ActionButton::kMaxButtons * sizeof(uint32_t) + 1);

  data.SetReadPos(0);
  EXPECT_EQ(data.Read<uint32_t>(), bar[0]);
  for (size_t i = 1; i < ActionButton::kMaxButtons; ++i)
    EXPECT_EQ(data.Read<uint32_t>(), 0u);
  EXPECT_EQ(data.Read<uint8>(), 0u);
}
