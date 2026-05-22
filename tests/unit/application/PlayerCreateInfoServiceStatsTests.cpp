#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <application/services/PlayerCreateInfoService.h>
#include <domain/models/Character.h>
#include <domain/models/PlayerTemplateStats.h>
#include <domain/repositories/IPlayerCreateInfoRepository.h>
#include <shared/Logger.h>

#include <algorithm>
#include <string>

using namespace Firelands;
using namespace testing;

namespace {

std::string const kTestDbcDir =
    std::string(FIRELANDS_TEST_DATA_DIR) + "/data/dbc";

} // namespace

class MockPlayerCreateInfoRepo : public IPlayerCreateInfoRepository {
public:
  MOCK_METHOD(std::optional<PlayerCreateInfo>, GetStartPosition, (uint8, uint8),
              (override));
  MOCK_METHOD(std::vector<PlayerCreateVisualItem>, GetVisualItems,
              (uint8, uint8, uint8, uint8), (override));
  MOCK_METHOD(std::vector<StarterItemGrant>, GetExtraCreateItems, (uint8, uint8),
              (override));
  MOCK_METHOD(std::vector<uint32_t>, GetStarterSpells, (uint8_t, uint8_t),
              (override));
  MOCK_METHOD(std::vector<StarterSkillGrant>, GetStarterSkills, (uint8_t, uint8_t),
              (override));
  MOCK_METHOD(std::optional<PlayerClassLevelStats>, GetClassLevelStats,
              (uint8_t, uint8_t), (override));
  MOCK_METHOD(std::optional<PlayerRaceStats>, GetRaceStats, (uint8_t),
              (override));
  MOCK_METHOD(uint32_t, GetXpForNextLevel, (uint8_t), (const, override));
};

TEST(PlayerCreateInfoServiceStats, AppliesClassRaceAndSetsHealth) {
  auto repo = std::make_shared<MockPlayerCreateInfoRepo>();
  PlayerCreateInfoService svc(repo, "", "");

  PlayerClassLevelStats cls;
  cls.str = 20;
  cls.agi = 10;
  cls.sta = 30;
  cls.inte = 12;
  cls.spi = 8;

  PlayerRaceStats race;
  race.str = 2;
  race.sta = 1;
  race.agi = race.inte = race.spi = 0;

  EXPECT_CALL(*repo, GetClassLevelStats(1, 3))
      .WillOnce(Return(std::optional<PlayerClassLevelStats>{cls}));
  EXPECT_CALL(*repo, GetRaceStats(1)).WillOnce(Return(std::optional<PlayerRaceStats>{race}));

  Character ch(1u, 1u, "T", 1, 1, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0,
               true);
  ASSERT_TRUE(svc.TryApplyTemplateCombatState(ch));
  EXPECT_EQ(ch.GetPrimaryStat(0), 22u);
  EXPECT_EQ(ch.GetPrimaryStat(2), 31u);
  EXPECT_GT(ch.GetMaxHealth(), 50u);
  EXPECT_EQ(ch.GetPowerType(), 1u); // warrior rage
}

TEST(PlayerCreateInfoServiceStats, MergesDbcSkillLineAndRacialSpellsWithWorldDb) {
  if (!Logger::IsInitialized())
    Logger::Init(LoggerBuilder().WithConsole(false).Build());
  auto repo = std::make_shared<MockPlayerCreateInfoRepo>();
  EXPECT_CALL(*repo, GetStarterSpells(2, 1))
      .WillOnce(Return(std::vector<uint32_t>{78u, 2457u}));

  PlayerCreateInfoService svc(repo, "", kTestDbcDir);
  std::vector<uint32_t> spells = svc.GetStarterSpells(2, 1);
  ASSERT_FALSE(spells.empty());

  auto has = [&](uint32_t id) {
    return std::find(spells.begin(), spells.end(), id) != spells.end();
  };
  EXPECT_TRUE(has(78u));
  EXPECT_TRUE(has(2457u));
  EXPECT_TRUE(has(20572u)) << "Orc Blood Fury from SkillLineAbility.dbc";
  EXPECT_TRUE(has(6603u)) << "Attack from starter weapon skill lines";
  EXPECT_TRUE(has(3018u)) << "Shoot from skill-value ability rows";
  EXPECT_TRUE(has(2764u)) << "Throw from skill-value ability rows";
}

TEST(PlayerCreateInfoServiceStats, ReturnsFalseWithoutClassRow) {
  auto repo = std::make_shared<MockPlayerCreateInfoRepo>();
  PlayerCreateInfoService svc(repo, "", "");
  EXPECT_CALL(*repo, GetClassLevelStats(9, 1))
      .WillOnce(Return(std::optional<PlayerClassLevelStats>{}));

  Character ch(1u, 1u, "T", 1, 9, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
               true);
  EXPECT_FALSE(svc.TryApplyTemplateCombatState(ch));
  EXPECT_EQ(ch.GetMaxHealth(), 100u);
}
