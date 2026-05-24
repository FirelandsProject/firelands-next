#include <domain/world/MapSnapshot.h>
#include <gtest/gtest.h>

using namespace Firelands;

TEST(MapSnapshotTests, DefaultIsEmpty) {
  MapSnapshot snap;
  EXPECT_EQ(snap.mapId, 0u);
  EXPECT_EQ(snap.playerCount, 0);
  EXPECT_EQ(snap.creatureCount, 0);
  EXPECT_EQ(snap.loadedGridCells, 0);
  EXPECT_DOUBLE_EQ(snap.avgTickTimeMs, 0.0);
  EXPECT_DOUBLE_EQ(snap.lastTickTimeMs, 0.0);
  EXPECT_TRUE(snap.isEmpty);
}

TEST(MapSnapshotTests, PopulatedSnapshotCanBeNonEmpty) {
  MapSnapshot snap{};
  snap.mapId = 1;
  snap.playerCount = 2;
  snap.creatureCount = 1;
  snap.loadedGridCells = 4;
  snap.avgTickTimeMs = 1.5;
  snap.lastTickTimeMs = 2.0;
  snap.isEmpty = false;
  EXPECT_FALSE(snap.isEmpty);
  EXPECT_EQ(snap.mapId, 1u);
  EXPECT_EQ(snap.playerCount, 2);
  EXPECT_EQ(snap.creatureCount, 1);
  EXPECT_EQ(snap.loadedGridCells, 4);
}
