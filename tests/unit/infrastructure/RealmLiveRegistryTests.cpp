#include <gtest/gtest.h>
#include <infrastructure/network/realm_link/RealmLiveRegistry.h>

using namespace Firelands;

TEST(RealmLiveRegistry, ClaimRelease) {
  RealmLiveRegistry reg;
  EXPECT_FALSE(reg.IsWorldConnected(7));
  EXPECT_EQ(reg.tryClaim(7), RealmLiveRegistry::ClaimResult::Ok);
  EXPECT_TRUE(reg.IsWorldConnected(7));
  EXPECT_EQ(reg.tryClaim(7), RealmLiveRegistry::ClaimResult::AlreadyOnline);
  reg.release(7);
  EXPECT_FALSE(reg.IsWorldConnected(7));
  EXPECT_EQ(reg.tryClaim(7), RealmLiveRegistry::ClaimResult::Ok);
}
