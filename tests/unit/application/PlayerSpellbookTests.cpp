#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <algorithm>
#include <application/services/PlayerSpellbook.h>
#include <application/services/PlayerCreateInfoService.h>
#include <domain/models/SpellDefinition.h>
#include <domain/repositories/IPlayerCreateInfoRepository.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <shared/Logger.h>
#include <shared/game/SkillLineCategories.h>

using namespace Firelands;
using namespace testing;

class PlayerSpellbookTest : public ::testing::Test {
protected:
  void SetUp() override {
    if (!Logger::IsInitialized())
      Logger::Init(LoggerBuilder().WithConsole(false).Build());
    LoadSkillLineCategories(std::string(FIRELANDS_TEST_DATA_DIR) +
                            "/data/dbc/SkillLine.dbc");
  }
};

class MockSpellStore : public ISpellDefinitionStore {
public:
  MOCK_METHOD(bool, HasSpell, (uint32), (const, override));
  MOCK_METHOD(std::optional<SpellDefinition>, GetDefinition, (uint32),
              (const, override));
};

class MockPciRepo : public IPlayerCreateInfoRepository {
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

TEST_F(PlayerSpellbookTest, FiltersExtraCharacterSpellsByRequiredLevel) {
  auto repo = std::make_shared<MockPciRepo>();
  EXPECT_CALL(*repo, GetStarterSpells(_, _))
      .WillRepeatedly(Return(std::vector<uint32_t>{}));
  PlayerCreateInfoService svc(repo, "", "");

  MockSpellStore spells;
  SpellDefinition low;
  low.id = 118;
  low.requiredLevel = 1;
  SpellDefinition high;
  high.id = 116;
  high.requiredLevel = 10;

  EXPECT_CALL(spells, GetDefinition(_)).WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(spells, GetDefinition(118)).WillRepeatedly(Return(low));
  EXPECT_CALL(spells, GetDefinition(116)).WillRepeatedly(Return(high));
  EXPECT_CALL(spells, HasSpell(_)).WillRepeatedly(Return(true));

  auto known =
      PlayerSpellbook::BuildKnownSpells(1, 8, 5, svc, &spells, {118u, 116u});
  EXPECT_THAT(known, Contains(118u));
  EXPECT_THAT(known, Not(Contains(116u)));
}

TEST_F(PlayerSpellbookTest, StarterSpellsFilteredByCharacterLevel) {
  auto repo = std::make_shared<MockPciRepo>();
  EXPECT_CALL(*repo, GetStarterSpells(8, 11))
      .WillRepeatedly(Return(std::vector<uint32_t>{5185u, 5176u, 8921u}));
  PlayerCreateInfoService svc(repo, "", "");

  MockSpellStore spells;
  SpellDefinition healingTouch;
  healingTouch.id = 5185;
  healingTouch.requiredLevel = 78;
  SpellDefinition wrath;
  wrath.id = 5176;
  wrath.requiredLevel = 1;
  SpellDefinition moonfire;
  moonfire.id = 8921;
  moonfire.requiredLevel = 4;

  EXPECT_CALL(spells, GetDefinition(_)).WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(spells, GetDefinition(5185))
      .WillRepeatedly(Return(healingTouch));
  EXPECT_CALL(spells, GetDefinition(5176)).WillRepeatedly(Return(wrath));
  EXPECT_CALL(spells, GetDefinition(8921)).WillRepeatedly(Return(moonfire));
  EXPECT_CALL(spells, HasSpell(_)).WillRepeatedly(Return(true));

  auto known =
      PlayerSpellbook::BuildKnownSpells(8, 11, 1, svc, &spells, {});
  EXPECT_THAT(known, Contains(5176u)) << "Wrath at level 1";
  EXPECT_THAT(known, Not(Contains(8921u))) << "Moonfire requires level 4";
  EXPECT_THAT(known, Not(Contains(5185u))) << "Healing Touch requires level 78";

  auto known5 =
      PlayerSpellbook::BuildKnownSpells(8, 11, 5, svc, &spells, {});
  EXPECT_THAT(known5, Contains(5176u));
  EXPECT_THAT(known5, Contains(8921u)) << "Moonfire unlocked at level 5";
  EXPECT_THAT(known5, Not(Contains(5185u))) << "Healing Touch still locked";
}

TEST_F(PlayerSpellbookTest, OmitsMountSpellsButKeepsCataClassAbilitiesIn864xxRange) {
  auto repo = std::make_shared<MockPciRepo>();
  EXPECT_CALL(*repo, GetStarterSpells(1, 1))
      .WillOnce(Return(std::vector<uint32_t>{78u, 86479u, 40120u}));

  PlayerCreateInfoService svc(repo, "", "");

  MockSpellStore spells;
  SpellDefinition warriorSpell;
  warriorSpell.id = 86479;
  SpellDefinition mountSpell;
  mountSpell.id = 40120;
  mountSpell.hasMountOrVehicleAura = true;
  SpellDefinition strike;
  strike.id = 78;

  EXPECT_CALL(spells, GetDefinition(_)).WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(spells, GetDefinition(86479)).WillRepeatedly(Return(warriorSpell));
  EXPECT_CALL(spells, GetDefinition(40120)).WillRepeatedly(Return(mountSpell));
  EXPECT_CALL(spells, GetDefinition(78)).WillRepeatedly(Return(strike));
  EXPECT_CALL(spells, HasSpell(_)).WillRepeatedly(Return(true));

  auto known = PlayerSpellbook::BuildKnownSpells(1, 1, 1, svc, &spells, {});
  EXPECT_THAT(known, Contains(78u));
  EXPECT_THAT(known, Contains(86479u))
      << "Cata class abilities in 864xx range must not be stripped";
  EXPECT_THAT(known, Not(Contains(40120u)))
      << "Mount/flying spells must not appear in spellbook";
}

TEST_F(PlayerSpellbookTest, FiltersPersistedMountAndProfessionSpells) {
  auto repo = std::make_shared<MockPciRepo>();
  EXPECT_CALL(*repo, GetStarterSpells(_, _))
      .WillRepeatedly(Return(std::vector<uint32_t>{}));
  PlayerCreateInfoService svc(repo, "", "");

  MockSpellStore spells;
  SpellDefinition mount;
  mount.id = 55531;
  mount.hasMountOrVehicleAura = true;
  SpellDefinition alchemy;
  alchemy.id = 2259;
  alchemy.grantsSkillLine = true;
  SpellDefinition strike;
  strike.id = 78;

  EXPECT_CALL(spells, GetDefinition(_)).WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(spells, GetDefinition(55531)).WillRepeatedly(Return(mount));
  EXPECT_CALL(spells, GetDefinition(2259)).WillRepeatedly(Return(alchemy));
  EXPECT_CALL(spells, GetDefinition(78)).WillRepeatedly(Return(strike));
  EXPECT_CALL(spells, HasSpell(_)).WillRepeatedly(Return(true));

  auto known = PlayerSpellbook::BuildKnownSpells(1, 1, 1, svc, &spells, {55531u, 2259u, 78u});
  EXPECT_THAT(known, Contains(78u));
  EXPECT_THAT(known, Not(Contains(55531u)));
  EXPECT_THAT(known, Not(Contains(2259u)));
}

TEST_F(PlayerSpellbookTest, StarterSkillsOmitMetaProfessionsAndFixZeroRanks) {
  auto repo = std::make_shared<MockPciRepo>();
  EXPECT_CALL(*repo, GetStarterSkills(1, 2))
      .WillOnce(Return(std::vector<StarterSkillGrant>{
          {95u, 0, 0},    // Defense
          {777u, 0, 0},   // Mounts meta
          {129u, 4, 4},   // First Aid
          {754u, 0, 0},   // Human racial (secondary)
          {413u, 0, 0},   // Mail
      }));
  PlayerCreateInfoService svc(repo, "", "");

  auto skills = PlayerSpellbook::BuildStarterSkills(1, 2, svc);
  auto hasSkill = [&](uint32_t id) {
    return std::any_of(skills.begin(), skills.end(),
                       [id](StarterSkillGrant const &g) {
                         return g.skillId == id;
                       });
  };
  EXPECT_TRUE(hasSkill(95u));
  EXPECT_TRUE(hasSkill(413u));
  EXPECT_FALSE(hasSkill(777u));
  EXPECT_FALSE(hasSkill(129u));
  EXPECT_FALSE(hasSkill(754u));

  for (StarterSkillGrant const &g : skills) {
    if (g.skillId == 98u || g.skillId == 754u)
      continue;
    EXPECT_LE(g.rank, g.maxRank);
    if (g.rank != 300u)
      EXPECT_NE(g.maxRank, 300u) << "Non-language skills must not use 300 cap";
  }
}
