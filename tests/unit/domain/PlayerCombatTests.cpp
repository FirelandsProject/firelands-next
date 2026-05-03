#include <gtest/gtest.h>
#include <domain/world/Player.h>
#include <application/ports/IMapNotifier.h>
#include <shared/network/WorldPacket.h>

using namespace Firelands;

namespace {

class NullNotifier final : public IMapNotifier {
public:
  void SendPacket(WorldPacket & /*packet*/) override {}
  uint64 GetGuid() const override { return 0; }
};

} // namespace

TEST(PlayerCombatTests, InitCombatResources_ClampsHealthToMax) {
  auto n = std::make_shared<NullNotifier>();
  Player p(1ull, n);
  p.InitCombatResources(500u, 100u);
  EXPECT_EQ(p.GetLiveMaxHealth(), 100u);
  EXPECT_EQ(p.GetLiveHealth(), 100u);
}

TEST(PlayerCombatTests, ApplyHealthDelta_Clamped) {
  auto n = std::make_shared<NullNotifier>();
  Player p(2ull, n);
  p.InitCombatResources(80u, 100u);
  p.ApplyHealthDelta(-200);
  EXPECT_EQ(p.GetLiveHealth(), 0u);
  p.ApplyHealthDelta(500);
  EXPECT_EQ(p.GetLiveHealth(), 100u);
}
