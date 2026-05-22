#include <gtest/gtest.h>
#include <application/services/CharacterActionButtons.h>
#include <domain/models/Character.h>
#include <shared/game/ActionButton.h>
#include <shared/game/InventorySlots.h>
#include <unordered_set>

using namespace Firelands;

TEST(CharacterActionButtonsTests, ApplyAndBuildPersistedStateRoundTrip) {
  ActionButton::PackedActionBar bar{};
  CharacterActionButtonState loaded;
  loaded.buttons.push_back(PersistedActionButton{2, 133u, ActionButton::Spell});
  loaded.buttons.push_back(PersistedActionButton{10, 40u, ActionButton::Macro});

  ApplyPersistedActionButtons(bar, loaded);
  EXPECT_EQ(bar[2], ActionButton::PackWire(133u, ActionButton::Spell));
  EXPECT_EQ(bar[10], ActionButton::PackWire(40u, ActionButton::Macro));
  EXPECT_EQ(bar[0], 0u);

  CharacterActionButtonState const saved = BuildPersistedActionButtons(bar, 0);
  ASSERT_EQ(saved.buttons.size(), 2u);
  EXPECT_EQ(saved.buttons[0].button, 2);
  EXPECT_EQ(saved.buttons[0].action, 133u);
  EXPECT_EQ(saved.buttons[1].button, 10);
}

TEST(CharacterActionButtonsTests, IsValidPlacementRequiresKnownSpell) {
  Character ch(1, 1, "Test", 1, 8, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, false);
  std::unordered_set<uint32_t> known{133u};
  EXPECT_TRUE(IsValidActionButtonPlacement(ActionButton::Spell, 133u, known, ch));
  EXPECT_FALSE(IsValidActionButtonPlacement(ActionButton::Spell, 999u, known, ch));
}

TEST(CharacterActionButtonsTests, IsValidPlacementRequiresItemInBag0) {
  Character ch(1, 1, "Test", 1, 8, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, false);
  std::array<uint32_t, kPackSlotCount> pack{};
  pack[0] = 6948u;
  Character withItem(1, 1, "Test", 1, 8, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                     false, 0, "", {}, {}, {}, pack, {}, {});
  std::unordered_set<uint32_t> known;
  EXPECT_TRUE(
      IsValidActionButtonPlacement(ActionButton::Item, 6948u, known, withItem));
  EXPECT_FALSE(IsValidActionButtonPlacement(ActionButton::Item, 6948u, known, ch));
}

TEST(CharacterActionButtonsTests, BuildPersistedUsesUint32WireSlots) {
  ActionButton::PackedActionBar bar{};
  bar[1] = ActionButton::PackWire(42u, ActionButton::Macro);
  CharacterActionButtonState const saved = BuildPersistedActionButtons(bar, 1);
  ASSERT_EQ(saved.buttons.size(), 1u);
  EXPECT_EQ(saved.buttons[0].button, 1);
  EXPECT_EQ(saved.buttons[0].action, 42u);
  EXPECT_EQ(saved.buttons[0].type, ActionButton::Macro);
}
