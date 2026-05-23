#include <gtest/gtest.h>
#include <shared/network/MovementInfo.h>
#include <shared/network/MovementStateQueries.h>

using namespace Firelands;

TEST(MovementStateQueries, SwimmingAndWalk) {
  MovementInfo m{};
  EXPECT_FALSE(MovementIsSwimming(m));
  EXPECT_FALSE(MovementPrefersWalkSpeed(m));
  EXPECT_EQ(MovementAnimTier(m), 0u);
  m.flags = MOVEMENTFLAG_SWIMMING;
  EXPECT_TRUE(MovementIsSwimming(m));
  EXPECT_EQ(MovementAnimTier(m), 1u);
  m.flags |= MOVEMENTFLAG_WALKING;
  EXPECT_TRUE(MovementPrefersWalkSpeed(m));
}

TEST(MovementStateQueries, AnimTierPrefersSwimOverFly) {
  MovementInfo m{};
  m.flags = MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_FLYING;
  EXPECT_EQ(MovementAnimTier(m), 1u);
  m.flags = MOVEMENTFLAG_FLYING;
  EXPECT_EQ(MovementAnimTier(m), 3u);
}

TEST(MovementStateQueries, AnimTierForLiquidBeforeSwimFlag) {
  MovementInfo m{};
  EXPECT_EQ(MovementAnimTierForLiquid(m, true), 1u);
  EXPECT_EQ(MovementAnimTierForLiquid(m, false), 0u);
}

TEST(MovementStateQueries, FlyingMotionUsesMask) {
  MovementInfo m{};
  m.flags = MOVEMENTFLAG_CAN_FLY;
  EXPECT_TRUE(MovementCanFly(m));
  EXPECT_FALSE(MovementIsFlyingMotion(m));
  m.flags |= MOVEMENTFLAG_FLYING;
  EXPECT_TRUE(MovementIsFlyingMotion(m));
  m.flags = MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_ASCENDING;
  EXPECT_TRUE(MovementIsFlyingMotion(m));
}

TEST(MovementStateQueries, FatiguePlayerFlag) {
  EXPECT_FALSE(PlayerFlagsIndicatesFatigueBoundary(0));
  EXPECT_TRUE(PlayerFlagsIndicatesFatigueBoundary(kPlayerFlagsIsOutOfBounds));
}
