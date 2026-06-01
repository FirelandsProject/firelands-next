#include <gtest/gtest.h>
#include <shared/game/StarterSpellFilters.h>

#include <algorithm>

using namespace Firelands;

TEST(StarterSpellFiltersTests, GuildPerkSpellIds) {
  EXPECT_TRUE(IsGuildPerkSpell(83951u));
  EXPECT_TRUE(IsGuildPerkSpell(83968u));
  EXPECT_TRUE(IsGuildPerkSpell(78631u));
  EXPECT_FALSE(IsGuildPerkSpell(635u));
  EXPECT_FALSE(IsGuildPerkSpell(6603u));
}

TEST(StarterSpellFiltersTests, WarlockQuestGatedSummonSpells) {
  EXPECT_TRUE(IsWarlockQuestGatedSummonSpell(688u));
  EXPECT_TRUE(IsWarlockQuestGatedSummonSpell(697u));
  EXPECT_FALSE(IsWarlockQuestGatedSummonSpell(686u));
  std::vector<uint32_t> const ids = WarlockQuestGatedSummonSpellIds();
  EXPECT_GE(ids.size(), 1u);
  EXPECT_NE(std::find(ids.begin(), ids.end(), 688u), ids.end());
}