#include <gtest/gtest.h>
#include <infrastructure/persistence/DatabaseService.h>
#include <infrastructure/persistence/MySqlPlayerQuestProgressRepository.h>

#include <memory>

namespace Firelands {
namespace {

class MySqlPlayerQuestProgressRepositoryTest : public ::testing::Test {
protected:
  void SetUp() override {
    try {
      DatabaseService dbService("tcp://127.0.0.1:3306/firelands_characters", "root",
                                "root");
      _connection = dbService.CreateConnection();
      _repository = std::make_unique<MySqlPlayerQuestProgressRepository>(_connection);

      auto stmt = std::shared_ptr<sql::Statement>(_connection->createStatement());
      stmt->execute("DELETE FROM `character_queststatus` WHERE `guid` = 900001");
      stmt->execute(
          "DELETE FROM `character_queststatus_rewarded` WHERE `guid` = 900001");
    } catch (std::exception const &e) {
      GTEST_SKIP() << "Database connection failed. Is Docker running? Error: "
                   << e.what();
    }
  }

  void TearDown() override {
    if (!_connection)
      return;
    auto stmt = std::shared_ptr<sql::Statement>(_connection->createStatement());
    stmt->execute("DELETE FROM `character_queststatus` WHERE `guid` = 900001");
    stmt->execute(
        "DELETE FROM `character_queststatus_rewarded` WHERE `guid` = 900001");
  }

  std::shared_ptr<sql::Connection> _connection;
  std::unique_ptr<MySqlPlayerQuestProgressRepository> _repository;
};

TEST_F(MySqlPlayerQuestProgressRepositoryTest, RoundTripsActiveAndRewardedQuests) {
  PlayerQuestProgressSnapshot snap;
  snap.activeQuests.emplace(24764, QuestStatus::Incomplete);
  snap.activeQuests.emplace(100, QuestStatus::Complete);
  snap.rewardedQuests.insert(42);

  ASSERT_TRUE(_repository->SaveForCharacter(900001, snap));

  PlayerQuestProgressSnapshot loaded = _repository->LoadForCharacter(900001);
  ASSERT_EQ(loaded.activeQuests.size(), 2u);
  EXPECT_EQ(loaded.activeQuests.at(24764), QuestStatus::Incomplete);
  EXPECT_EQ(loaded.activeQuests.at(100), QuestStatus::Complete);
  ASSERT_EQ(loaded.rewardedQuests.size(), 1u);
  EXPECT_TRUE(loaded.rewardedQuests.count(42) != 0);
}

TEST_F(MySqlPlayerQuestProgressRepositoryTest, SaveReplacesPreviousRows) {
  PlayerQuestProgressSnapshot first;
  first.activeQuests.emplace(10, QuestStatus::Incomplete);
  ASSERT_TRUE(_repository->SaveForCharacter(900001, first));

  PlayerQuestProgressSnapshot second;
  second.rewardedQuests.insert(10);
  ASSERT_TRUE(_repository->SaveForCharacter(900001, second));

  PlayerQuestProgressSnapshot loaded = _repository->LoadForCharacter(900001);
  EXPECT_TRUE(loaded.activeQuests.empty());
  ASSERT_EQ(loaded.rewardedQuests.size(), 1u);
  EXPECT_TRUE(loaded.rewardedQuests.count(10) != 0);
}

} // namespace
} // namespace Firelands
