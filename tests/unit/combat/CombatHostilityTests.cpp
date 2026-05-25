#include <gtest/gtest.h>
#include <application/combat/CombatHostility.h>
#include <domain/ports/IMapNotifier.h>
#include <domain/world/Creature.h>
#include <domain/world/Player.h>

using namespace Firelands;

namespace {

class NullNotifier final : public IMapNotifier {
public:
  void SendPacket(WorldPacket & /*packet*/) override {}
  uint64 GetGuid() const override { return 0; }
};

} // namespace

TEST(CombatHostilityTests, SameRaceAlliancePlayersCannotMelee) {
  auto n = std::make_shared<NullNotifier>();
  Player attacker(1ull, n);
  Player target(2ull, n);
  attacker.SetRaceAndFaction(1, 1);
  target.SetRaceAndFaction(3, 1);
  EXPECT_FALSE(application::CanMeleeAttack(attacker, target));
}

TEST(CombatHostilityTests, CrossFactionPlayersCanMelee) {
  auto n = std::make_shared<NullNotifier>();
  Player attacker(1ull, n);
  Player target(2ull, n);
  attacker.SetRaceAndFaction(1, 1);
  target.SetRaceAndFaction(2, 2);
  EXPECT_TRUE(application::CanMeleeAttack(attacker, target));
}

TEST(CombatHostilityTests, PlayerCanMeleeCreatureWithoutDbc) {
  auto n = std::make_shared<NullNotifier>();
  Player attacker(1ull, n);
  attacker.SetRaceAndFaction(1, 1);
  Creature target(9ull, 1u, 1u, 100u, 1u, 7u);
  EXPECT_TRUE(application::CanMeleeAttack(attacker, target, nullptr));
}

TEST(CombatHostilityTests, EvadingCreatureCannotBeMeleeAttacked) {
  auto n = std::make_shared<NullNotifier>();
  Player attacker(1ull, n);
  attacker.SetRaceAndFaction(1, 1);
  Creature target(9ull, 1u, 1u, 100u, 1u, 7u);
  target.SetEvading(true);
  EXPECT_FALSE(application::CanMeleeAttack(attacker, target, nullptr));
}
