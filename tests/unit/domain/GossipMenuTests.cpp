#include <domain/models/GossipMenu.h>
#include <gtest/gtest.h>

namespace Firelands {

TEST(GossipMenuTest, EmptyMenuInitially) {
  GossipMenu menu;
  EXPECT_TRUE(menu.Empty());
  EXPECT_EQ(menu.GetItemCount(), 0u);
}

TEST(GossipMenuTest, AddItemIncreasesCount) {
  GossipMenu menu;
  menu.menuId = 100;
  GossipMenuItem item;
  item.menuId = 100;
  item.optionIndex = 0;
  item.icon = GossipOptionIcon::Chat;
  item.optionText = "Hello";
  menu.items.push_back(item);

  EXPECT_FALSE(menu.Empty());
  EXPECT_EQ(menu.GetItemCount(), 1u);
}

TEST(GossipMenuTest, GetItemByIndex) {
  GossipMenu menu;
  menu.menuId = 100;

  GossipMenuItem item0;
  item0.menuId = 100;
  item0.optionIndex = 0;
  item0.optionText = "First";
  menu.items.push_back(item0);

  GossipMenuItem item1;
  item1.menuId = 100;
  item1.optionIndex = 1;
  item1.optionText = "Second";
  menu.items.push_back(item1);

  auto const *found = menu.GetItem(1);
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->optionText, "Second");
  EXPECT_EQ(found->optionIndex, 1u);
}

TEST(GossipMenuTest, GetItemNotFoundReturnsNull) {
  GossipMenu menu;
  EXPECT_EQ(menu.GetItem(99), nullptr);

  GossipMenuItem item;
  item.optionIndex = 0;
  menu.items.push_back(item);

  EXPECT_EQ(menu.GetItem(5), nullptr);
}

TEST(GossipMenuTest, ClearRemovesAllItems) {
  GossipMenu menu;
  GossipMenuItem item;
  item.optionIndex = 0;
  menu.items.push_back(item);
  menu.items.push_back(item);

  EXPECT_EQ(menu.GetItemCount(), 2u);
  menu.Clear();
  EXPECT_TRUE(menu.Empty());
  EXPECT_EQ(menu.GetItemCount(), 0u);
}

TEST(GossipQuestItemTest, DefaultValues) {
  GossipQuestItem qi;
  EXPECT_EQ(qi.questId, 0u);
  EXPECT_EQ(qi.questIcon, 0);
  EXPECT_EQ(qi.questLevel, 0);
  EXPECT_EQ(qi.questFlags, 0u);
  EXPECT_FALSE(qi.blueQuestionMark);
  EXPECT_TRUE(qi.questTitle.empty());
}

TEST(GossipMenuItemTest, DefaultValues) {
  GossipMenuItem mi;
  EXPECT_EQ(mi.menuId, 0u);
  EXPECT_EQ(mi.optionIndex, 0u);
  EXPECT_EQ(mi.icon, GossipOptionIcon::Chat);
  EXPECT_FALSE(mi.isCoded);
  EXPECT_EQ(mi.boxMoney, 0u);
  EXPECT_EQ(mi.optionType, GossipOptionType::None);
  EXPECT_EQ(mi.actionMenuId, 0u);
  EXPECT_EQ(mi.actionPoi, 0u);
  EXPECT_EQ(mi.sender, 0u);
  EXPECT_EQ(mi.action, 0u);
}

} // namespace Firelands
