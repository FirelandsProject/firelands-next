#include <gtest/gtest.h>
#include <shared/game/StarterSpellFilters.h>

using namespace Firelands;

TEST(StarterSpellFiltersTests, RidingSpellsAreExcluded) {
  EXPECT_TRUE(IsRidingOrTransportStarterSpell(40120u));
  EXPECT_TRUE(IsRidingOrTransportStarterSpell(86470u));
  EXPECT_FALSE(IsRidingOrTransportStarterSpell(20572u));
}

TEST(StarterSpellFiltersTests, ShapeshiftSpellsAreExcluded) {
  EXPECT_TRUE(IsClassShapeshiftStarterSpell(768u));
  EXPECT_FALSE(IsClassShapeshiftStarterSpell(5176u));
}

TEST(StarterSpellFiltersTests, MountAuraTypesAreExcludedFromLogin) {
  EXPECT_TRUE(IsMountOrVehicleAuraType(32u));
  EXPECT_TRUE(IsMountOrVehicleAuraType(78u));
  EXPECT_FALSE(IsMountOrVehicleAuraType(36u));
  EXPECT_TRUE(IsExcludedLoginAuraType(78u));
  EXPECT_TRUE(IsExcludedLoginAuraType(36u));
  EXPECT_FALSE(IsExcludedLoginAuraType(99u));
}

