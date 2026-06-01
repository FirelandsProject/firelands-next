#include <gtest/gtest.h>
#include <shared/game/StarterSkillFilters.h>

using namespace Firelands;

TEST(StarterSkillFiltersTests, MetaSkillsAreExcluded) {
  EXPECT_TRUE(IsMetaOrInternalStarterSkill(95u));
  EXPECT_TRUE(IsMetaOrInternalStarterSkill(183u));
  EXPECT_TRUE(IsMetaOrInternalStarterSkill(777u));
  EXPECT_TRUE(IsMetaOrInternalStarterSkill(778u));
  EXPECT_TRUE(IsMetaOrInternalStarterSkill(810u));
  EXPECT_FALSE(IsMetaOrInternalStarterSkill(43u));
  EXPECT_FALSE(IsMetaOrInternalStarterSkill(415u));
}

TEST(StarterSkillFiltersTests, DefenseSkillExcludedFromStarterUi) {
  EXPECT_TRUE(IsMetaOrInternalStarterSkill(95u));
  EXPECT_TRUE(IsExcludedStarterSkill(95u));
}

TEST(StarterSkillFiltersTests, ProfessionsAndRidingAreExcluded) {
  EXPECT_TRUE(IsProfessionStarterSkill(129u));
  EXPECT_TRUE(IsRidingStarterSkill(762u));
  EXPECT_FALSE(IsProfessionStarterSkill(43u));
}

TEST(StarterSkillFiltersTests, ClassSpellTabsAreExcluded) {
  EXPECT_TRUE(IsClassSpellTabStarterSkill(800u));
  EXPECT_FALSE(IsClassSpellTabStarterSkill(413u));
}

TEST(StarterSkillFiltersTests, WeaponSkillsAreKept) {
  EXPECT_FALSE(IsExcludedStarterSkill(43u));
  EXPECT_FALSE(IsExcludedStarterSkill(415u));
}