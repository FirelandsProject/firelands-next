#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <domain/world/Creature.h>
#include <domain/world/Map.h>
#include <domain/world/Player.h>
#include <domain/ports/IMapNotifier.h>
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

TEST_F(MapTests, TryGetCreature_ReturnsCreature) {
  auto map = std::make_shared<Map>(1);
  auto creature = std::make_shared<Creature>(999u, 42u, 15688u);
  map->AddObject(creature);
  auto got = map->TryGetCreature(999u);
  ASSERT_TRUE(got);
  EXPECT_EQ(got->GetGuid(), 999u);
  EXPECT_FALSE(map->TryGetCreature(1u));
}

TEST_F(MapTests, TryGetCreature_ReturnsNullForPlayer) {
  auto map = std::make_shared<Map>(1);
  auto notifier = std::make_shared<MockNotifier>();
  auto player = std::make_shared<Player>(1u, notifier);
  map->AddObject(player);
  EXPECT_FALSE(map->TryGetCreature(1u));
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

TEST_F(MapTests, TryGetObjectWorldPosition_ReturnsCoordsWhenPresent) {
  auto map = std::make_shared<Map>(1);
  auto notifier = std::make_shared<MockNotifier>();
  auto player = std::make_shared<Player>(77, notifier);
  MovementInfo m{};
  m.x = 12.5f;
  m.y = -3.f;
  m.z = 100.f;
  player->SetPosition(m);
  map->AddObject(player);
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
  ASSERT_TRUE(map->TryGetObjectWorldPosition(77, x, y, z));
  EXPECT_FLOAT_EQ(x, 12.5f);
  EXPECT_FLOAT_EQ(y, -3.f);
  EXPECT_FLOAT_EQ(z, 100.f);
}

TEST_F(MapTests, TryGetObjectWorldPosition_FalseWhenMissing) {
  auto map = std::make_shared<Map>(1);
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
  EXPECT_FALSE(map->TryGetObjectWorldPosition(999, x, y, z));
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

TEST_F(MapTests, ForEachPlayer_CallbackCanBroadcastWithoutDeadlock) {
  auto map = std::make_shared<Map>(1);
  auto notifier1 = std::make_shared<MockNotifier>();
  auto player1 = std::make_shared<Player>(1, notifier1);
  auto notifier2 = std::make_shared<MockNotifier>();
  auto player2 = std::make_shared<Player>(2, notifier2);
  map->AddObject(player1);
  map->AddObject(player2);

  EXPECT_CALL(*notifier2, SendPacket(_)).Times(1);
  map->ForEachPlayer([&](std::shared_ptr<Player> const &p) {
    if (p->GetGuid() != 1)
      return;
    WorldPacket packet(0x4707, 0);
    map->BroadcastPacket(1, packet, false);
  });
}

TEST_F(MapTests, CreateSnapshot_CountsPlayersCreaturesAndGridCells) {
  auto map = std::make_shared<Map>(42);
  auto notifier = std::make_shared<MockNotifier>();
  map->AddObject(std::make_shared<Player>(1u, notifier));
  map->AddObject(std::make_shared<Creature>(2u, 1u, 1u));

  MapSnapshot const snap = map->CreateSnapshot();
  EXPECT_EQ(snap.mapId, 42u);
  EXPECT_EQ(snap.playerCount, 1);
  EXPECT_EQ(snap.creatureCount, 1);
  EXPECT_GE(snap.loadedGridCells, 1);
  EXPECT_FALSE(snap.isEmpty);
}

TEST_F(MapTests, CreateSnapshot_EmptyMap) {
  auto map = std::make_shared<Map>(7);
  MapSnapshot const snap = map->CreateSnapshot();
  EXPECT_EQ(snap.mapId, 7u);
  EXPECT_TRUE(snap.isEmpty);
  EXPECT_EQ(snap.loadedGridCells, 0);
}

TEST_F(MapTests, IsEmpty_IgnoresGameObjectsOnly) {
  auto map = std::make_shared<Map>(1);
  EXPECT_TRUE(map->IsEmpty());
  map->AddObject(std::make_shared<Creature>(2u, 1u, 1u));
  EXPECT_FALSE(map->IsEmpty());
}

TEST_F(MapTests, CountPlayersAndCreatures_WithoutGridScan) {
  auto map = std::make_shared<Map>(1);
  auto notifier = std::make_shared<MockNotifier>();
  map->AddObject(std::make_shared<Player>(1u, notifier));
  map->AddObject(std::make_shared<Creature>(2u, 1u, 1u));
  EXPECT_EQ(map->CountPlayers(), 1);
  EXPECT_EQ(map->CountCreatures(), 1);
}

TEST_F(MapTests, RecordTickTime_UpdatesRollingAverage) {
  auto map = std::make_shared<Map>(1);
  map->RecordTickTime(10.0);
  map->RecordTickTime(20.0);
  MapSnapshot const snap = map->CreateSnapshot();
  EXPECT_DOUBLE_EQ(snap.lastTickTimeMs, 20.0);
  EXPECT_GT(snap.avgTickTimeMs, 10.0);
  EXPECT_LT(snap.avgTickTimeMs, 20.0);
}
