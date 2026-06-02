#include <gtest/gtest.h>
#include <infrastructure/world/MapCollisionQueriesStub.h>

using namespace Firelands;

TEST(MapCollisionQueriesStub, LineOfSightAlwaysOpen) {
  MapCollisionQueriesStub stub("");
  EXPECT_TRUE(stub.LineOfSight(0, 0, 0, 0, 10, 10, 10));
}

TEST(MapCollisionQueriesStub, NavMeshUnavailableWhenDataRootEmpty) {
  MapCollisionQueriesStub empty("");
  EXPECT_FALSE(empty.IsNavMeshDataAvailable(530));

  MapCollisionQueriesStub withData("/tmp/maps");
  EXPECT_TRUE(withData.IsNavMeshDataAvailable(0));
}

TEST(MapCollisionQueriesStub, FindPathReturnsNavMeshMissingWhenEmpty) {
  MapCollisionQueriesStub stub("");
  FindPathRequest req;
  req.mapId = 0;
  req.startX = 0; req.startY = 0; req.startZ = 0;
  req.endX = 10; req.endY = 10; req.endZ = 0;
  auto const result = stub.FindPath(req);
  EXPECT_EQ(result.status, FindPathStatus::NavMeshMissing);
  EXPECT_TRUE(result.waypoints.empty());
}

TEST(MapCollisionQueriesStub, FindPathReturnsDirectLineWhenDataRootSet) {
  MapCollisionQueriesStub stub("/data/maps");
  FindPathRequest req;
  req.mapId = 0;
  req.startX = 0; req.startY = 0; req.startZ = 0;
  req.endX = 10; req.endY = 0; req.endZ = 0;
  auto const result = stub.FindPath(req);
  EXPECT_EQ(result.status, FindPathStatus::Complete);
  ASSERT_EQ(result.waypoints.size(), 2u);
  EXPECT_FLOAT_EQ(result.waypoints[0].x, 0.0f);
  EXPECT_FLOAT_EQ(result.waypoints[1].x, 10.0f);
}

TEST(MapCollisionQueriesStub, GetHeightReturnsHint) {
  MapCollisionQueriesStub stub("");
  EXPECT_FLOAT_EQ(stub.GetHeight(0, 100.0f, 200.0f, 50.0f), 50.0f);
  EXPECT_FLOAT_EQ(stub.GetHeight(0, 100.0f, 200.0f, -10.0f), -10.0f);
}
