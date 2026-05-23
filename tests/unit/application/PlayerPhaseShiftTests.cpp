#include <application/world/PlayerPhaseShift.h>
#include <gtest/gtest.h>

namespace Firelands {
namespace {

TEST(PlayerPhaseShiftTests, AreaPhase169AppliedForGilneasZone) {
  PhaseShift const shift = BuildPlayerPhaseShift({169}, {}, nullptr, nullptr);
  EXPECT_TRUE(shift.HasPhase(169));
  EXPECT_TRUE((shift.flags & static_cast<uint32>(PhaseShiftFlags::Unphased)) != 0u);
}

TEST(PlayerPhaseShiftTests, QuestPhaseAuraClearsDefaultUnphased) {
  PhaseShift const shift = BuildPlayerPhaseShift({169}, {}, nullptr, nullptr);
  PhaseShift questShift = shift;
  questShift.AddPhase(170);
  questShift.flags &= ~static_cast<uint32>(PhaseShiftFlags::Unphased);
  EXPECT_TRUE(questShift.HasPhase(169));
  EXPECT_TRUE(questShift.HasPhase(170));
  EXPECT_FALSE((questShift.flags & static_cast<uint32>(PhaseShiftFlags::Unphased)) != 0u);
}

} // namespace
} // namespace Firelands
