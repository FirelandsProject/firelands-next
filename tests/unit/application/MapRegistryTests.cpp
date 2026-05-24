#include <application/services/MapRegistry.h>
#include <gtest/gtest.h>

using namespace Firelands;

TEST(MapRegistryTests, GetOrCreateReturnsSameService) {
  MapRegistry registry;
  auto a = registry.GetOrCreate(0);
  auto b = registry.GetOrCreate(0);
  EXPECT_EQ(a.get(), b.get());
  EXPECT_EQ(a->MapId(), 0u);
}

TEST(MapRegistryTests, TryGetReturnsNullForUnknownMap) {
  MapRegistry registry;
  EXPECT_FALSE(registry.TryGet(99));
}

TEST(MapRegistryTests, AllSnapshotsIncludesRegisteredMaps) {
  MapRegistry registry;
  (void)registry.GetOrCreate(0);
  (void)registry.GetOrCreate(1);
  auto const snaps = registry.AllSnapshots();
  EXPECT_EQ(snaps.size(), 2u);
}

TEST(MapRegistryTests, ClearRemovesServices) {
  MapRegistry registry;
  (void)registry.GetOrCreate(0);
  registry.Clear();
  EXPECT_FALSE(registry.TryGet(0));
  EXPECT_TRUE(registry.AllSnapshots().empty());
}
