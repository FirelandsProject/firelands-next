#include <gtest/gtest.h>
#include <shared/game/SkillLineCategories.h>
#include <shared/game/StarterSkillFilters.h>
#include <shared/Logger.h>

using namespace Firelands;

class SkillLineCategoriesTest : public ::testing::Test {
protected:
  void SetUp() override {
    if (!Logger::IsInitialized())
      Logger::Init(LoggerBuilder().WithConsole(false).Build());
    LoadSkillLineCategories(std::string(FIRELANDS_TEST_DATA_DIR) + "/data/dbc/SkillLine.dbc");
  }
};

TEST_F(SkillLineCategoriesTest, AllowsWeaponArmorLanguageOnly) {
  ASSERT_TRUE(SkillLineCategoriesLoaded());
  // Skill wire slots: weapon (6), armor (8), language (10) only.
  EXPECT_TRUE(IsAllowedStarterSkillLine(43u));   // Swords → cat 6
  EXPECT_TRUE(IsAllowedStarterSkillLine(415u));  // Cloth → cat 8
  EXPECT_TRUE(IsAllowedStarterSkillLine(98u));   // Common lang → cat 10
  EXPECT_FALSE(IsAllowedStarterSkillLine(754u)); // Human racial → cat 9 (no wire slot)
  EXPECT_FALSE(IsAllowedStarterSkillLine(777u)); // Mounts meta → cat 7
  EXPECT_FALSE(IsAllowedStarterSkillLine(171u)); // Alchemy → cat 11
}

TEST_F(SkillLineCategoriesTest, SpellGrantsAllowRacialBlockProfession) {
  ASSERT_TRUE(SkillLineCategoriesLoaded());
  // Spells: profession and generic blocked; racial/class allowed.
  EXPECT_TRUE(IsExcludedSpellGrantSkillLine(821u));  // Guild perks → block
  EXPECT_TRUE(IsExcludedSpellGrantSkillLine(777u)); // Mount collection meta → block
  EXPECT_TRUE(IsExcludedSpellGrantSkillLine(778u)); // Companion pets meta → block
  EXPECT_TRUE(IsExcludedSpellGrantSkillLine(594u)); // Paladin class tab → block
  EXPECT_FALSE(IsExcludedSpellGrantSkillLine(754u)); // Human racial → keep spell
  EXPECT_FALSE(IsExcludedSpellGrantSkillLine(43u));  // Swords → keep
  EXPECT_TRUE(IsExcludedSpellGrantSkillLine(171u));  // Alchemy → block
  EXPECT_TRUE(IsExcludedSpellGrantSkillLine(333u));  // Enchanting → block
  // Secondary professions (cat 9, same as racials) must also be blocked explicitly.
  EXPECT_TRUE(IsExcludedSpellGrantSkillLine(129u));  // First Aid → block
  EXPECT_TRUE(IsExcludedSpellGrantSkillLine(185u));  // Cooking → block
  EXPECT_TRUE(IsExcludedSpellGrantSkillLine(356u));  // Fishing → block
  EXPECT_TRUE(IsExcludedSpellGrantSkillLine(762u));  // Riding → block
}

TEST_F(SkillLineCategoriesTest, ExcludesRacialFromSkillWireSlots) {
  ASSERT_TRUE(SkillLineCategoriesLoaded());
  EXPECT_TRUE(IsExcludedStarterSkill(777u));   // Mounts meta → blocked from wire
  EXPECT_TRUE(IsExcludedStarterSkill(754u));   // Human racial → blocked from wire
  EXPECT_FALSE(IsExcludedStarterSkill(95u));   // Defense → weapon cat → allowed
}
