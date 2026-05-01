#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <domain/world/Creature.h>
#include <domain/world/Map.h>
#include <domain/world/Player.h>
#include <application/ports/IMapNotifier.h>
#include <shared/network/WorldPacket.h>

#include <shared/Logger.h>

using namespace Firelands;
using namespace testing;

class MapTests : public Test {
protected:
    void SetUp() override {
        if (!Logger::IsInitialized()) {
            Logger::Init(LoggerBuilder().WithConsole(false).Build());
        }
    }
};

class MockNotifier : public IMapNotifier {
public:
    MOCK_METHOD(void, SendPacket, (WorldPacket&), (override));
    MOCK_METHOD(uint64, GetGuid, (), (const, override));
};

TEST_F(MapTests, BroadcastPacket_SendsToOtherPlayers) {
    auto map = std::make_shared<Map>(1);
    
    auto notifier1 = std::make_shared<MockNotifier>();
    auto player1 = std::make_shared<Player>(1, notifier1);
    
    auto notifier2 = std::make_shared<MockNotifier>();
    auto player2 = std::make_shared<Player>(2, notifier2);
    
    map->AddObject(player1);
    map->AddObject(player2);
    
    WorldPacket packet(0x1234, 0);
    
    // Player 1 broadcasts, Player 2 should receive
    EXPECT_CALL(*notifier2, SendPacket(_)).Times(1);
    EXPECT_CALL(*notifier1, SendPacket(_)).Times(0);
    
    map->BroadcastPacket(1, packet, false);
}

TEST_F(MapTests, BroadcastPacket_IncludeSelf_SendsToAll) {
    auto map = std::make_shared<Map>(1);
    
    auto notifier1 = std::make_shared<MockNotifier>();
    auto player1 = std::make_shared<Player>(1, notifier1);
    
    map->AddObject(player1);
    
    WorldPacket packet(0x1234, 0);
    
    EXPECT_CALL(*notifier1, SendPacket(_)).Times(1);
    
    map->BroadcastPacket(1, packet, true);
}

TEST_F(MapTests, AddCreature_DoesNotBreakBroadcastToPlayers) {
  auto map = std::make_shared<Map>(1);
  auto notifier1 = std::make_shared<MockNotifier>();
  auto player1 = std::make_shared<Player>(1, notifier1);
  auto notifier2 = std::make_shared<MockNotifier>();
  auto player2 = std::make_shared<Player>(2, notifier2);
  MovementInfo pos{};
  pos.x = 0.f;
  pos.y = 0.f;
  player1->SetPosition(pos);
  player2->SetPosition(pos);

  auto creature = std::make_shared<Creature>(999, 42, 15688);
  creature->SetPosition(pos);

  map->AddObject(player1);
  map->AddObject(player2);
  map->AddObject(creature);

  WorldPacket packet(0x1234, 0);
  EXPECT_CALL(*notifier2, SendPacket(_)).Times(1);
  EXPECT_CALL(*notifier1, SendPacket(_)).Times(0);
  map->BroadcastPacket(1, packet, false);
}

TEST_F(MapTests, ForEachPlayer_VisitsOnlyPlayers) {
  auto map = std::make_shared<Map>(1);
  auto notifier1 = std::make_shared<MockNotifier>();
  auto player1 = std::make_shared<Player>(1, notifier1);
  auto notifier2 = std::make_shared<MockNotifier>();
  auto player2 = std::make_shared<Player>(2, notifier2);
  map->AddObject(player1);
  map->AddObject(player2);

  int count = 0;
  map->ForEachPlayer([&](std::shared_ptr<Player> const &p) {
    ++count;
    EXPECT_TRUE(p->GetGuid() == 1 || p->GetGuid() == 2);
  });
  EXPECT_EQ(count, 2);
}

TEST_F(MapTests, BroadcastPacketToNearby_DoesNotSendToFarPlayers) {
    auto map = std::make_shared<Map>(1);
    
    auto notifier1 = std::make_shared<MockNotifier>();
    auto player1 = std::make_shared<Player>(1, notifier1);
    MovementInfo pos1; pos1.x = 0; pos1.y = 0; // Middle (Cell 32, 32)
    player1->SetPosition(pos1);
    
    auto notifier2 = std::make_shared<MockNotifier>();
    auto player2 = std::make_shared<Player>(2, notifier2);
    MovementInfo pos2; pos2.x = 1000; pos2.y = 1000; // Far (Cell 30, 30 approx)
    player2->SetPosition(pos2);
    
    map->AddObject(player1);
    map->AddObject(player2);
    
    WorldPacket packet(0x1234, 0);
    
    // Player 1 broadcasts nearby, Player 2 is far away (more than 1 cell)
    EXPECT_CALL(*notifier2, SendPacket(_)).Times(0);
    
    map->BroadcastPacketToNearby(1, packet, false);
}
