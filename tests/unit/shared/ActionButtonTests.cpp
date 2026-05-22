#include <gtest/gtest.h>
#include <shared/game/ActionButton.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <unordered_set>

using namespace Firelands;
using namespace Firelands::ActionButton;

TEST(ActionButtonTests, PackUnpackRoundTrip) {
  uint32_t const packed = Pack(133u, Spell);
  EXPECT_EQ(ActionFromPacked(packed), 133u);
  EXPECT_EQ(TypeFromPacked(packed), Spell);
}

TEST(ActionButtonTests, IsValidSetRequestRejectsOutOfRangeButton) {
  EXPECT_FALSE(IsValidSetRequest(kMaxButtons, PackWire(1u, Spell)));
  EXPECT_TRUE(IsValidSetRequest(0, 0u));
}

TEST(ActionButtonTests, IsValidSetRequestRejectsUnknownType) {
  uint32_t const bad = PackWire(10u, 0x99);
  EXPECT_FALSE(IsValidSetRequest(0, bad));
}

TEST(ActionButtonTests, PackWireMatchesCataclysmUint32Slot) {
  uint32_t const wire = PackWire(133u, Spell);
  EXPECT_EQ(ActionFromPacked(PackFromClientAction(wire)), 133u);
  EXPECT_EQ(TypeFromPacked(PackFromClientAction(wire)), Spell);
}

TEST(ActionButtonTests, MayPlaceOnBarRequiresKnownSpell) {
  std::unordered_set<uint32_t> known{133u};
  EXPECT_TRUE(MayPlaceOnBar(Spell, 133u, known));
  EXPECT_FALSE(MayPlaceOnBar(Spell, 16857u, known));
  EXPECT_TRUE(MayPlaceOnBar(Macro, 1u, known));
}

TEST(ActionButtonTests, TryParseSetActionButtonCmsgReadsTrinityLayout) {
  WorldPacket packet(CMSG_SET_ACTION_BUTTON);
  uint32_t const packed = Pack(133u, Spell);
  packet.Append<uint32_t>(packed);
  packet.Append<uint8_t>(3);

  SetActionButtonCmsg req;
  ASSERT_TRUE(TryParseSetActionButtonCmsg(packet, req));
  EXPECT_EQ(req.index, 3);
  EXPECT_EQ(req.packedAction, packed);
}

TEST(ActionButtonTests, TryParseSetActionButtonCmsgReadsIndexFirstLayout) {
  WorldPacket packet(CMSG_SET_ACTION_BUTTON);
  uint32_t const packed = Pack(16857u, Spell);
  packet.Append<uint8_t>(5);
  packet.Append<uint32_t>(packed);

  SetActionButtonCmsg req;
  ASSERT_TRUE(TryParseSetActionButtonCmsg(packet, req));
  EXPECT_EQ(req.index, 5);
  EXPECT_EQ(req.packedAction, packed);
}

TEST(ActionButtonTests, ClearBarZerosAllSlots) {
  PackedActionBar bar{};
  bar[3] = PackWire(5u, Spell);
  ClearBar(bar);
  EXPECT_EQ(bar[3], 0u);
}
