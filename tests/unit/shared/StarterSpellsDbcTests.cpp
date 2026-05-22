#include <gtest/gtest.h>
#include <shared/dbc/StarterSpellsDbc.h>
#include <shared/game/SkillLineCategories.h>
#include <shared/Logger.h>

#include <algorithm>
#include <string>

using namespace Firelands;

namespace {

std::string const kDbcDir =
    std::string(FIRELANDS_TEST_DATA_DIR) + "/data/dbc";

class StarterSpellsDbcTests : public ::testing::Test {
protected:
  void SetUp() override {
    if (!Logger::IsInitialized())
      Logger::Init(LoggerBuilder().WithConsole(false).Build());
    LoadSkillLineCategories(kDbcDir + "/SkillLine.dbc");
  }
};

} // namespace

TEST_F(StarterSpellsDbcTests, LoadMissingFile_ReturnsFalse) {
  StarterSpellsDbc dbc;
  EXPECT_FALSE(dbc.Load("/nonexistent/SkillLineAbility.dbc",
                        "/nonexistent/SkillRaceClassInfo.dbc"));
  EXPECT_FALSE(dbc.IsLoaded());
}

TEST_F(StarterSpellsDbcTests, LoadBundledDbc_TrollDruidExcludesClassTabSpells) {
  StarterSpellsDbc dbc;
  ASSERT_TRUE(dbc.Load(kDbcDir + "/SkillLineAbility.dbc",
                        kDbcDir + "/SkillRaceClassInfo.dbc"));
  ASSERT_TRUE(dbc.IsLoaded());

  std::vector<uint32_t> const spells = dbc.GetStarterSpells(8, 11);
  std::vector<uint32_t> const weaponArmor =
      dbc.GetWeaponArmorLanguageStarterSpells(8, 11);
  ASSERT_FALSE(spells.empty());

  auto has = [&](std::vector<uint32_t> const &v, uint32_t id) {
    return std::find(v.begin(), v.end(), id) != v.end();
  };
  EXPECT_FALSE(has(spells, 5176u)) << "Wrath comes from playercreateinfo, not DBC";
  EXPECT_FALSE(has(spells, 5185u)) << "Healing Touch";
  EXPECT_FALSE(has(spells, 8921u)) << "Moonfire";
  EXPECT_FALSE(has(spells, 768u)) << "Cat Form";
  EXPECT_TRUE(has(weaponArmor, 6603u)) << "Attack";
  EXPECT_FALSE(has(spells, 33388u)) << "riding spell";
}

TEST_F(StarterSpellsDbcTests, LoadBundledDbc_HumanWarriorMatchesReferenceSubset) {
  StarterSpellsDbc dbc;
  ASSERT_TRUE(dbc.Load(kDbcDir + "/SkillLineAbility.dbc",
                        kDbcDir + "/SkillRaceClassInfo.dbc"));

  std::vector<uint32_t> spells = dbc.GetStarterSpells(1, 1);
  ASSERT_FALSE(spells.empty());

  auto has = [&](uint32_t id) {
    return std::find(spells.begin(), spells.end(), id) != spells.end();
  };
  EXPECT_TRUE(has(78u));
  EXPECT_TRUE(has(2457u));
  EXPECT_TRUE(has(6673u));
}

TEST_F(StarterSpellsDbcTests, GetRacialSpells_OrcHasBloodFury) {
  StarterSpellsDbc dbc;
  ASSERT_TRUE(dbc.Load(kDbcDir + "/SkillLineAbility.dbc",
                        kDbcDir + "/SkillRaceClassInfo.dbc"));

  std::vector<uint32_t> orcWarrior = dbc.GetRacialSpells(2, 1);
  auto has = [&](uint32_t id) {
    return std::find(orcWarrior.begin(), orcWarrior.end(), id) !=
           orcWarrior.end();
  };
  EXPECT_TRUE(has(20572u)) << "Blood Fury";
  EXPECT_FALSE(has(78u)) << "class spell Sinister Strike should not be racial-only";

  std::vector<uint32_t> humanWarrior = dbc.GetRacialSpells(1, 1);
  EXPECT_TRUE(std::find(humanWarrior.begin(), humanWarrior.end(), 20572u) ==
              humanWarrior.end())
      << "Human should not get Orc Blood Fury";
}

TEST_F(StarterSpellsDbcTests, HumanWarriorExcludesMountCollectionSpells) {
  StarterSpellsDbc dbc;
  ASSERT_TRUE(dbc.Load(kDbcDir + "/SkillLineAbility.dbc",
                        kDbcDir + "/SkillRaceClassInfo.dbc"));

  std::vector<uint32_t> spells = dbc.GetStarterSpells(1, 1);
  auto has = [&](uint32_t id) {
    return std::find(spells.begin(), spells.end(), id) != spells.end();
  };
  EXPECT_FALSE(has(40192u)) << "Swift Purple Wind Rider";
  EXPECT_FALSE(has(458u)) << "Brown Horse";
  EXPECT_FALSE(has(579u)) << "Red Wolf";
  EXPECT_TRUE(IsExcludedSpellGrantSkillLine(777u));
}

TEST_F(StarterSpellsDbcTests, HumanPaladinExcludesGuildPerkSpells) {
  StarterSpellsDbc dbc;
  ASSERT_TRUE(dbc.Load(kDbcDir + "/SkillLineAbility.dbc",
                        kDbcDir + "/SkillRaceClassInfo.dbc"));

  std::vector<uint32_t> spells = dbc.GetWeaponArmorLanguageStarterSpells(1, 2);
  auto has = [&](uint32_t id) {
    return std::find(spells.begin(), spells.end(), id) != spells.end();
  };
  EXPECT_FALSE(has(83951u)) << "Bartering guild perk";
  EXPECT_FALSE(has(83968u)) << "Fast Track guild perk";
  EXPECT_TRUE(dbc.IsSpellFromExcludedSkillLine(83951u));
  EXPECT_TRUE(IsExcludedSpellGrantSkillLine(821u)) << "Guild perk skill line";
}

TEST_F(StarterSpellsDbcTests, HumanPaladinWeaponArmorLanguageStarterSet) {
  StarterSpellsDbc dbc;
  ASSERT_TRUE(dbc.Load(kDbcDir + "/SkillLineAbility.dbc",
                        kDbcDir + "/SkillRaceClassInfo.dbc"));

  std::vector<uint32_t> const all = dbc.GetStarterSpells(1, 2);
  std::vector<uint32_t> const narrow = dbc.GetWeaponArmorLanguageStarterSpells(1, 2);
  auto has = [&](std::vector<uint32_t> const &v, uint32_t id) {
    return std::find(v.begin(), v.end(), id) != v.end();
  };

  EXPECT_LT(narrow.size(), all.size());
  EXPECT_TRUE(has(narrow, 203u)) << "Unarmed";
  EXPECT_TRUE(has(narrow, 8737u)) << "Mail armor proficiency";
  EXPECT_FALSE(has(narrow, 635u)) << "Holy Light comes from playercreateinfo, not class tab";
  EXPECT_FALSE(has(narrow, 20271u)) << "Judgment is a class-tab spell";
  EXPECT_FALSE(has(narrow, 83951u)) << "Guild perk";
}

TEST_F(StarterSpellsDbcTests, OrcWarriorIncludesShootAndThrowAtSkillRankOne) {
  StarterSpellsDbc dbc;
  ASSERT_TRUE(dbc.Load(kDbcDir + "/SkillLineAbility.dbc",
                        kDbcDir + "/SkillRaceClassInfo.dbc"));

  std::vector<uint32_t> spells =
      dbc.GetWeaponArmorLanguageStarterSpells(2, 1);
  auto has = [&](uint32_t id) {
    return std::find(spells.begin(), spells.end(), id) != spells.end();
  };
  EXPECT_TRUE(has(6603u)) << "Attack (GENERIC skill line 183)";
  EXPECT_TRUE(has(3018u)) << "Shoot (acquire on skill value)";
  EXPECT_TRUE(has(2764u)) << "Throw (acquire on skill value)";
}

TEST_F(StarterSpellsDbcTests, GetRacialSpells_BloodElfPaladinHasArcaneTorrent) {
  StarterSpellsDbc dbc;
  ASSERT_TRUE(dbc.Load(kDbcDir + "/SkillLineAbility.dbc",
                        kDbcDir + "/SkillRaceClassInfo.dbc"));

  std::vector<uint32_t> spells = dbc.GetRacialSpells(10, 2);
  EXPECT_TRUE(std::find(spells.begin(), spells.end(), 28730u) != spells.end())
      << "Arcane Torrent";
}
