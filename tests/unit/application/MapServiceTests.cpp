#include <application/services/MapService.h>
#include <domain/world/Creature.h>
#include <domain/world/Player.h>
#include <domain/ports/IMapNotifier.h>
#include <shared/network/WorldPacket.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace Firelands;
using namespace testing;

namespace {

class MockNotifier : public IMapNotifier {
public:
  MOCK_METHOD(void, SendPacket, (WorldPacket &), (override));
  MOCK_METHOD(uint64, GetGuid, (), (const, override));
};

} // namespace

TEST(MapServiceTests, SnapshotReflectsMapOccupancy) {
  auto map = std::make_shared<Map>(5);
  map->AddObject(std::make_shared<Creature>(10u, 1u, 1u));
  MapService svc(5, map);

  MapSnapshot const snap = svc.Snapshot();
  EXPECT_EQ(snap.mapId, 5u);
  EXPECT_EQ(snap.creatureCount, 1);
  EXPECT_FALSE(snap.isEmpty);
}

TEST(MapServiceTests, RecordTickUpdatesSnapshotTiming) {
  auto map = std::make_shared<Map>(1);
  MapService svc(1, map);
  svc.RecordTick(12.5);
  svc.RecordTick(7.5);

  MapSnapshot const snap = svc.Snapshot();
  EXPECT_DOUBLE_EQ(snap.lastTickTimeMs, 7.5);
  EXPECT_GT(snap.avgTickTimeMs, 7.5);
  EXPECT_LT(snap.avgTickTimeMs, 12.5);
}
