#include <gtest/gtest.h>
#include <domain/Unit.h>

TEST(UnitAttributes, HealthInitialization) {
    using namespace Firelands;
    Unit unit(100); // 100 max health
    EXPECT_EQ(unit.GetHealth(), 100);
}

TEST(UnitAttributes, SetHealthCapsAtMax) {
    using namespace Firelands;
    Unit unit(100);
    unit.SetHealth(150);
    EXPECT_EQ(unit.GetHealth(), 100);
}
