#include <gtest/gtest.h>
#include <application/ports/IRealmLiveState.h>
#include <application/services/RealmListService.h>
#include <domain/repositories/IRealmRepository.h>
#include <domain/models/Realm.h>
#include <memory>
#include <vector>

using namespace Firelands;

class MockLiveRealm1Only : public IRealmLiveState {
public:
  bool IsWorldConnected(uint32_t realmId) const override {
    return realmId == 1;
  }
};

class MockRealmRepository : public IRealmRepository {
public:
    std::vector<Realm> GetRealms() override {
        return {
            Realm(1, "Test Realm 1", "127.0.0.1", 8085, 0, 1, 0, 1.0f),
            Realm(2, "Test Realm 2", "127.0.0.1", 8086, 0, 1, 0, 2.0f)
        };
    }
    
    bool FindById(uint32_t) override { return false; }
    void DeleteById(uint32_t) override {}
    void Create(const Realm&) override {}
};

TEST(RealmListService, FetchRealms) {
    auto repo = std::make_shared<MockRealmRepository>();
    RealmListService service(repo);
    
    auto realms = service.GetRealmList();
    ASSERT_EQ(realms.size(), 2);
    EXPECT_EQ(realms[0].GetName(), "Test Realm 1");
    EXPECT_EQ(realms[1].GetName(), "Test Realm 2");
}

TEST(RealmListService, LiveStateMarksDisconnectedRealmOffline) {
    auto repo = std::make_shared<MockRealmRepository>();
    auto live = std::make_shared<MockLiveRealm1Only>();
    RealmListService service(repo, live);

    auto realms = service.GetRealmList();
    ASSERT_EQ(realms.size(), 2);
    EXPECT_FLOAT_EQ(realms[0].GetPopulation(), 1.0f);
    EXPECT_EQ(realms[0].GetRealmListFlags(), 0u);
    EXPECT_FLOAT_EQ(realms[1].GetPopulation(), 0.0f);
    EXPECT_EQ(realms[1].GetRealmListFlags(), 2u);
}
