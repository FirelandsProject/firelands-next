#include <gtest/gtest.h>
#include <domain/models/Realm.h>
#include <stdexcept>

TEST(RealmDomain, ConstructorInitialization) {
    using namespace Firelands;
    Realm realm(1, "TestRealm", "127.0.0.1", 8085, 0, 1, 0, 100.0f);
    
    EXPECT_EQ(realm.GetId(), 1);
    EXPECT_EQ(realm.GetName(), "TestRealm");
    EXPECT_EQ(realm.GetAddress(), "127.0.0.1");
    EXPECT_EQ(realm.GetPort(), 8085);
    EXPECT_EQ(realm.GetIcon(), 0);
    EXPECT_EQ(realm.GetTimezone(), 1);
    EXPECT_EQ(realm.GetAllowedSecurityLevel(), 0);
    EXPECT_FLOAT_EQ(realm.GetPopulation(), 100.0f);
    EXPECT_EQ(realm.GetRealmListFlags(), 0u);
}

TEST(RealmDomain, HandlesNegativePopulation) {
    using namespace Firelands;
    Realm realm(1, "TestRealm", "127.0.0.1", 8085, 0, 1, 0, -50.0f);
    
    EXPECT_FLOAT_EQ(realm.GetPopulation(), 0.0f); // Negative population should be capped at 0
}

TEST(RealmDomain, ValidatesPort) {
    using namespace Firelands;
    EXPECT_THROW({
        Realm realm(1, "TestRealm", "127.0.0.1", 0, 0, 1, 0, 0.0f); // Port 0 is invalid
    }, std::invalid_argument);
}
