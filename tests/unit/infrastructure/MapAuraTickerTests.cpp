#include <application/services/WorldService.h>
#include <domain/world/Creature.h>
#include <gtest/gtest.h>
#include <infrastructure/world/MapAuraTicker.h>
#include <shared/Logger.h>

using namespace Firelands;

class MapAuraTickerTests : public ::testing::Test {
protected:
  void TearDown() override { WorldService::Instance().ResetForShutdown(); }
};

TEST_F(MapAuraTickerTests, TickMapAurasRecordsPerMapTickTime) {
  if (!Logger::IsInitialized()) {
    Logger::Init(LoggerBuilder().WithConsole(false).Build());
  }
  WorldService::Instance().AddCreatureToMap(
      0, std::make_shared<Creature>(1u, 1u, 1u));
  (void)WorldService::Instance().GetMap(1);

  auto const now = std::chrono::steady_clock::now();
  TickMapAuras(now);

  auto const snaps = WorldService::Instance().GetMapSnapshots();
  ASSERT_EQ(snaps.size(), 2u);
  auto tick_for = [&](uint32 mapId) {
    for (auto const &snap : snaps) {
      if (snap.mapId == mapId)
        return snap.lastTickTimeMs;
    }
    return 0.0;
  };
  EXPECT_GT(tick_for(0), 0.0);
  EXPECT_DOUBLE_EQ(tick_for(1), 0.0);
}
