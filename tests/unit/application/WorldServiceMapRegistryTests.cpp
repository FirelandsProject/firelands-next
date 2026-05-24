#include <application/services/WorldService.h>
#include <domain/world/Creature.h>
#include <gtest/gtest.h>

using namespace Firelands;

class WorldServiceMapRegistryTests : public ::testing::Test {
protected:
  void TearDown() override { WorldService::Instance().ResetForShutdown(); }
};

TEST_F(WorldServiceMapRegistryTests, GetMapReturnsSharedInstance) {
  auto a = WorldService::Instance().GetMap(0);
  auto b = WorldService::Instance().GetMap(0);
  EXPECT_EQ(a.get(), b.get());
}

TEST_F(WorldServiceMapRegistryTests, GetMapSnapshotsListsActiveMaps) {
  WorldService::Instance().AddCreatureToMap(
      42, std::make_shared<Creature>(1u, 1u, 1u));
  auto const snaps = WorldService::Instance().GetMapSnapshots();
  ASSERT_EQ(snaps.size(), 1u);
  EXPECT_EQ(snaps[0].mapId, 42u);
  EXPECT_EQ(snaps[0].creatureCount, 1);
}

TEST_F(WorldServiceMapRegistryTests, RemovePlayerFromMapDoesNotCreateMap) {
  WorldService::Instance().RemovePlayerFromMap(999, 1);
  EXPECT_TRUE(WorldService::Instance().GetMapSnapshots().empty());
}

TEST_F(WorldServiceMapRegistryTests, ResetForShutdownClearsMaps) {
  (void)WorldService::Instance().GetMap(1);
  WorldService::Instance().ResetForShutdown();
  EXPECT_TRUE(WorldService::Instance().GetMapSnapshots().empty());
}
