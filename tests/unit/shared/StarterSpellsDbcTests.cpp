#include <gtest/gtest.h>
#include <shared/dbc/StarterSpellsDbc.h>
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
  }
};

} // namespace

TEST_F(StarterSpellsDbcTests, LoadMissingFile_ReturnsFalse) {
  StarterSpellsDbc dbc;
  EXPECT_FALSE(dbc.Load("/nonexistent/SkillLineAbility.dbc",
                        "/nonexistent/SkillRaceClassInfo.dbc"));
  EXPECT_FALSE(dbc.IsLoaded());
}

TEST_F(StarterSpellsDbcTests, LoadBundledDbc_TrollDruidHasCoreSpells) {
  StarterSpellsDbc dbc;
  ASSERT_TRUE(dbc.Load(kDbcDir + "/SkillLineAbility.dbc",
                        kDbcDir + "/SkillRaceClassInfo.dbc"));
  ASSERT_TRUE(dbc.IsLoaded());

  std::vector<uint32_t> spells = dbc.GetStarterSpells(8, 11);
  ASSERT_FALSE(spells.empty());

  auto has = [&](uint32_t id) {
    return std::find(spells.begin(), spells.end(), id) != spells.end();
  };
  EXPECT_TRUE(has(5176u)) << "Wrath";
  EXPECT_TRUE(has(5185u)) << "Healing Touch";
  EXPECT_TRUE(has(8921u)) << "Moonfire";
  EXPECT_TRUE(has(768u)) << "Cat Form";
  EXPECT_FALSE(has(33388u)) << "riding spell";
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

TEST_F(StarterSpellsDbcTests, GetRacialSpells_BloodElfPaladinHasArcaneTorrent) {
  StarterSpellsDbc dbc;
  ASSERT_TRUE(dbc.Load(kDbcDir + "/SkillLineAbility.dbc",
                        kDbcDir + "/SkillRaceClassInfo.dbc"));

  std::vector<uint32_t> spells = dbc.GetRacialSpells(10, 2);
  EXPECT_TRUE(std::find(spells.begin(), spells.end(), 28730u) != spells.end())
      << "Arcane Torrent";
}
