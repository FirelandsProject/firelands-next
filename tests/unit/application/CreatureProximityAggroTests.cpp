#include <gtest/gtest.h>
#include <application/combat/CreatureProximityAggro.h>
#include <application/ports/IMapNotifier.h>
#include <domain/world/Creature.h>
#include <domain/world/Player.h>
#include <shared/dbc/FactionTemplateDbc.h>
#include <shared/dbc/FactionTemplateHelpers.h>

using namespace Firelands;

namespace {

class NullNotifier final : public IMapNotifier {
public:
  void SendPacket(WorldPacket & /*packet*/) override {}
  uint64 GetGuid() const override { return 0; }
};

FactionTemplateEntry HostileMonsterFaction() {
  FactionTemplateEntry e{};
  e.faction = 14;
  e.enemyGroup = FactionGroupMaskPlayer;
  return e;
}

} // namespace

TEST(CreatureProximityAggroTests, InRangeWithinTwentyYards) {
  EXPECT_TRUE(application::IsWithinProximityAggroRange(0.f, 0.f, 0.f, 15.f, 0.f, 0.f));
  EXPECT_FALSE(application::IsWithinProximityAggroRange(0.f, 0.f, 0.f, 25.f, 0.f, 0.f));
}

TEST(CreatureProximityAggroTests, MonsterFactionGroupProvokesAggro) {
  FactionTemplateEntry monster{};
  monster.factionGroup = FactionGroupMaskMonster;
  EXPECT_TRUE(application::FactionProvokesProximityAggro(monster));
  FactionTemplateEntry vendor{};
  vendor.factionGroup = FactionGroupMaskAlliance;
  vendor.friendGroup = FactionGroupMaskPlayer;
  EXPECT_FALSE(application::FactionProvokesProximityAggro(vendor));
}

TEST(CreatureProximityAggroTests, PicksClosestHostilePlayer) {
  auto n = std::make_shared<NullNotifier>();
  Creature mob(100ull, 1u, 1u, 500u, 5u, 14u);
  auto nearPl = std::make_shared<Player>(1ull, n);
  auto farPl = std::make_shared<Player>(2ull, n);
  nearPl->SetRaceAndFaction(1, 1);
  farPl->SetRaceAndFaction(1, 1);
  MovementInfo mobPos{};
  mob.SetPosition(mobPos);

  FactionTemplateDbc dbc;
  FactionTemplateEntry playerTpl{};
  playerTpl.faction = 1;
  playerTpl.enemies[0] = 14;
  dbc.InjectEntryForTest(1, playerTpl);
  FactionTemplateEntry mobTpl = HostileMonsterFaction();
  mobTpl.factionGroup = FactionGroupMaskMonster;
  mobTpl.enemies[0] = 1;
  dbc.InjectEntryForTest(14, mobTpl);

  std::vector<application::ProximityAggroPlayer> players{
      {2ull, 18.f, 0.f, 0.f, farPl.get()},
      {1ull, 10.f, 0.f, 0.f, nearPl.get()},
  };
  auto const picked =
      application::PickClosestHostilePlayerInAggroRange(mob, players, &dbc);
  ASSERT_TRUE(picked.has_value());
  EXPECT_EQ(*picked, 1ull);
}

TEST(CreatureProximityAggroTests, SkipsWhenAlreadyChasing) {
  auto n = std::make_shared<NullNotifier>();
  Creature mob(100ull, 1u, 1u, 500u, 5u, 14u);
  mob.SetChaseTargetPlayerGuid(99ull);
  auto pl = std::make_shared<Player>(1ull, n);
  pl->SetRaceAndFaction(1, 1);
  MovementInfo mobPos{};
  mob.SetPosition(mobPos);

  FactionTemplateDbc dbc;
  FactionTemplateEntry mobTpl = HostileMonsterFaction();
  mobTpl.factionGroup = FactionGroupMaskMonster;
  dbc.InjectEntryForTest(14, mobTpl);

  std::vector<application::ProximityAggroPlayer> players{
      {1ull, 5.f, 0.f, 0.f, pl.get()},
  };
  EXPECT_FALSE(
      application::PickClosestHostilePlayerInAggroRange(mob, players, &dbc).has_value());
}
