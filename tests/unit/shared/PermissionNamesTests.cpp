#include <gtest/gtest.h>
#include <shared/game/PermissionNames.h>

using namespace Firelands;

TEST(PermissionNamesTests, ParsesNamedPermissions) {
  auto mask = ParsePermissionTokens({"gps", "teleport"});
  ASSERT_TRUE(mask.has_value());
  EXPECT_NE(*mask & ToMask(Permission::CommandGps), 0u);
  EXPECT_NE(*mask & ToMask(Permission::CommandTeleport), 0u);
}

TEST(PermissionNamesTests, ParsesHexMask) {
  auto mask = ParsePermissionTokens({"0x3"});
  ASSERT_TRUE(mask.has_value());
  EXPECT_EQ(*mask, 3u);
}

TEST(PermissionNamesTests, RejectsUnknownName) {
  EXPECT_FALSE(ParsePermissionTokens({"not_a_permission"}).has_value());
}
