#include <gtest/gtest.h>
#include <shared/dbc/FactionTemplateHelpers.h>

using namespace Firelands;

namespace {

FactionTemplateEntry MakeEntry(uint32_t flags, uint32_t friendGroup,
                               uint32_t enemyGroup) {
  FactionTemplateEntry e{};
  e.flags = flags;
  e.friendGroup = friendGroup;
  e.enemyGroup = enemyGroup;
  return e;
}

} // namespace

TEST(FactionTemplateHelpersTests, HostileWhenEnemyGroupIncludesPlayer) {
  auto e = MakeEntry(0, 0, FactionGroupMaskPlayer);
  EXPECT_TRUE(FactionTemplateHostileToPlayers(e));
  EXPECT_TRUE(FactionTemplateLikelyHostileToPlayers(e));
  EXPECT_FALSE(FactionTemplateLikelyFriendlyToPlayers(e));
}

TEST(FactionTemplateHelpersTests, FriendlyWhenFriendMaskAndNotEnemyToPlayer) {
  auto e = MakeEntry(0, FactionGroupMaskPlayer, 0);
  EXPECT_FALSE(FactionTemplateHostileToPlayers(e));
  EXPECT_TRUE(FactionTemplateLikelyFriendlyToPlayers(e));
  EXPECT_FALSE(FactionTemplateLikelyHostileToPlayers(e));
}

TEST(FactionTemplateHelpersTests, HatesAllExceptFriendsWithoutFriendIsHostile) {
  auto e = MakeEntry(FactionTemplateFlagHatesAllExceptFriends, 0, 0);
  EXPECT_TRUE(FactionTemplateLikelyHostileToPlayers(e));
}

TEST(FactionTemplateHelpersTests, HatesAllExceptFriendsWithFriendMaskNotHostile) {
  auto e = MakeEntry(FactionTemplateFlagHatesAllExceptFriends,
                     FactionGroupMaskPlayer, 0);
  EXPECT_FALSE(FactionTemplateLikelyHostileToPlayers(e));
}

TEST(FactionTemplateHelpersTests, NeutralHeuristic) {
  auto e = MakeEntry(0, 0, FactionGroupMaskMonster);
  EXPECT_FALSE(FactionTemplateLikelyHostileToPlayers(e));
  EXPECT_FALSE(FactionTemplateLikelyFriendlyToPlayers(e));
  EXPECT_TRUE(FactionTemplateLikelyNeutralToPlayers(e));
}
