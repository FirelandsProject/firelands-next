#include <gtest/gtest.h>
#include <shared/game/PhaseShift.h>

namespace Firelands {
namespace {

TEST(PhaseShiftTests, UnphasedViewerDoesNotSeePhasedCreature) {
  PhaseShift viewer;
  viewer.flags = static_cast<uint32>(PhaseShiftFlags::Unphased);

  PhaseShift creature;
  InitDbCreaturePhaseShift(creature, kPhaseUseFlagsNone, 170, 0,
                           [](uint32) { return std::vector<uint16>{}; });

  EXPECT_FALSE(viewer.CanSee(creature));
}

TEST(PhaseShiftTests, MatchingPhaseIdsAreVisible) {
  PhaseShift viewer;
  viewer.AddPhase(169);
  viewer.flags &= ~static_cast<uint32>(PhaseShiftFlags::Unphased);

  PhaseShift creature;
  InitDbCreaturePhaseShift(creature, kPhaseUseFlagsNone, 169, 0,
                           [](uint32) { return std::vector<uint16>{}; });

  EXPECT_TRUE(viewer.CanSee(creature));
}

TEST(PhaseShiftTests, AlwaysVisibleCreatureSeenByUnphasedPlayer) {
  PhaseShift viewer;
  viewer.flags = static_cast<uint32>(PhaseShiftFlags::Unphased);

  PhaseShift creature;
  InitDbCreaturePhaseShift(creature, kPhaseUseFlagsAlwaysVisible, 0, 0,
                           [](uint32) { return std::vector<uint16>{}; });

  EXPECT_TRUE(viewer.CanSee(creature));
}

TEST(PhaseShiftTests, DefaultPhaseSpawnIsUnphased) {
  PhaseShift creature;
  InitDbCreaturePhaseShift(creature, kPhaseUseFlagsNone, 0, 0,
                           [](uint32) { return std::vector<uint16>{}; });

  PhaseShift viewer;
  viewer.flags = static_cast<uint32>(PhaseShiftFlags::Unphased);

  EXPECT_TRUE(viewer.CanSee(creature));
}

} // namespace
} // namespace Firelands
