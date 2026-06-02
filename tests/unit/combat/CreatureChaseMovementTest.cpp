#include <application/combat/CreatureChaseMovement.h>
#include <application/ports/IMapCollisionQueries.h>
#include <gtest/gtest.h>
#include <shared/network/MovementFlags.h>

namespace {

Firelands::MovementInfo MakePos(float x, float y, float z) {
  Firelands::MovementInfo m{};
  m.x = x;
  m.y = y;
  m.z = z;
  return m;
}

class MockCollisionQueries : public Firelands::IMapCollisionQueries {
public:
  bool IsNavMeshDataAvailable(uint32_t) const override { return navMeshAvailable; }
  uint32_t GetLoadedMapCount() const override { return navMeshAvailable ? 1u : 0u; }
  uint32_t GetLoadedTileCount() const override { return navMeshAvailable ? 1u : 0u; }
  std::vector<std::pair<uint32_t, uint32_t>> GetLoadedTiles(uint32_t) const override {
    return {};
  }
  bool LineOfSight(uint32_t, float, float, float, float, float, float) const override {
    return true;
  }
  Firelands::FindPathResult FindPath(Firelands::FindPathRequest const&) const override {
    Firelands::FindPathResult result;
    if (!navMeshAvailable) {
      result.status = Firelands::FindPathStatus::NavMeshMissing;
      return result;
    }
    result.status = Firelands::FindPathStatus::Complete;
    result.waypoints.push_back({0.0f, 0.0f, 0.0f});
    result.waypoints.push_back({5.0f, 0.0f, 0.0f});
    result.waypoints.push_back({10.0f, 0.0f, 0.0f});
    return result;
  }
  float GetHeight(uint32_t, float, float, float zHint) const override { return zHint; }
  bool navMeshAvailable = false;
};

} // namespace

TEST(CreatureChaseMovementTest, StepsTowardTargetAtConfiguredSpeed) {
  application::combat::CreatureChaseConfig config{};
  config.runSpeedYardsPerSec = 10.0f;
  config.stopDistanceYards = 2.0f;

  auto const result = application::combat::StepCreatureTowardTarget(
      MakePos(0.f, 0.f, 0.f), 20.f, 0.f, 0.f, 0.5f, config);

  EXPECT_TRUE(result.moved);
  EXPECT_FALSE(result.inStopRange);
  EXPECT_NEAR(result.position.x, 5.f, 0.05f);
  EXPECT_EQ(result.position.flags, Firelands::MOVEMENTFLAG_FORWARD);
}

TEST(CreatureChaseMovementTest, StopsWithinMeleeStopDistance) {
  application::combat::CreatureChaseConfig config{};
  config.stopDistanceYards = 2.0f;

  auto const result = application::combat::StepCreatureTowardTarget(
      MakePos(0.f, 0.f, 0.f), 1.5f, 0.f, 0.f, 1.0f, config);

  EXPECT_FALSE(result.moved);
  EXPECT_TRUE(result.inStopRange);
  EXPECT_EQ(result.position.flags, Firelands::MOVEMENTFLAG_NONE);
}

TEST(CreatureChaseMovementTest, ProjectionAdvancesFartherThanSingleTick) {
  application::combat::CreatureChaseConfig config{};
  config.runSpeedYardsPerSec = 7.0f;
  config.stopDistanceYards = 2.0f;

  auto const single = application::combat::StepCreatureTowardTarget(
      MakePos(0.f, 0.f, 0.f), 30.f, 0.f, 0.f, 0.2f, config);
  auto const projected = application::combat::ProjectCreatureTowardTarget(
      MakePos(0.f, 0.f, 0.f), 30.f, 0.f, 0.f, 1.5f, config);

  EXPECT_TRUE(projected.moved);
  EXPECT_GT(projected.position.x, single.position.x);
}

TEST(CreatureChaseMovementTest, ChaseTargetRelocatedDetectsMeaningfulMove) {
  EXPECT_FALSE(application::combat::ChaseTargetRelocated(0.f, 0.f, 0.f, 0.2f, 0.f, 0.f));
  EXPECT_TRUE(application::combat::ChaseTargetRelocated(0.f, 0.f, 0.f, 2.f, 0.f, 0.f));
}

TEST(CreatureChaseMovementTest, StandPositionIsStopDistanceFromTarget) {
  auto const stand = application::combat::ComputeChaseStandPosition(
      MakePos(10.f, 0.f, 0.f), 0.f, 0.f, 0.f, 1.0f);
  EXPECT_NEAR(stand.x, 1.0f, 0.01f);
  EXPECT_NEAR(stand.y, 0.f, 0.01f);
}

TEST(CreatureChaseMovementTest, ComputeNavMeshPathReturnsEmptyWhenNoCollision) {
  auto const waypoints = application::combat::ComputeNavMeshPath(
      0, MakePos(0, 0, 0), 10, 0, 0, nullptr);
  EXPECT_TRUE(waypoints.empty());
}

TEST(CreatureChaseMovementTest, ComputeNavMeshPathReturnsEmptyWhenNavMeshMissing) {
  MockCollisionQueries mock;
  mock.navMeshAvailable = false;
  auto const waypoints = application::combat::ComputeNavMeshPath(
      0, MakePos(0, 0, 0), 10, 0, 0, &mock);
  EXPECT_TRUE(waypoints.empty());
}

TEST(CreatureChaseMovementTest, ComputeNavMeshPathReturnsWaypointsWhenAvailable) {
  MockCollisionQueries mock;
  mock.navMeshAvailable = true;
  auto const waypoints = application::combat::ComputeNavMeshPath(
      0, MakePos(0, 0, 0), 10, 0, 0, &mock);
  ASSERT_EQ(waypoints.size(), 3u);
  EXPECT_FLOAT_EQ(waypoints[0].x, 0.0f);
  EXPECT_FLOAT_EQ(waypoints[2].x, 10.0f);
}

TEST(CreatureChaseMovementTest, StepAlongNavMeshAdvancesToFirstWaypoint) {
  MockCollisionQueries mock;
  mock.navMeshAvailable = true;
  application::combat::CreatureChaseConfig config{};
  config.runSpeedYardsPerSec = 10.0f;
  config.stopDistanceYards = 1.0f;

  application::combat::ChaseNavMeshState state;
  state.lastTargetX = 10.0f;
  state.lastTargetY = 0.0f;
  state.lastTargetZ = 0.0f;
  state.waypoints.push_back(Firelands::Vec3{0.0f, 0.0f, 0.0f});
  state.waypoints.push_back(Firelands::Vec3{5.0f, 0.0f, 0.0f});
  state.waypoints.push_back(Firelands::Vec3{10.0f, 0.0f, 0.0f});
  state.currentWaypoint = 1;

  auto const result = application::combat::StepCreatureAlongNavMeshPath(
      MakePos(0.0f, 0.0f, 0.0f), 10.0f, 0.0f, 0.0f, 0.5f, config, state, &mock, 0u);

  EXPECT_TRUE(result.moved);
  EXPECT_GT(result.position.x, 0.0f);
  EXPECT_LT(result.position.x, 6.0f);
}

TEST(CreatureChaseMovementTest, StepAlongNavMeshFallsBackToDirectWhenNoWaypoints) {
  application::combat::CreatureChaseConfig config{};
  config.runSpeedYardsPerSec = 10.0f;
  config.stopDistanceYards = 2.0f;

  application::combat::ChaseNavMeshState state;
  auto const result = application::combat::StepCreatureAlongNavMeshPath(
      MakePos(0.0f, 0.0f, 0.0f), 20.0f, 0.0f, 0.0f, 0.5f, config, state, nullptr, 0u);

  EXPECT_TRUE(result.moved);
  EXPECT_NEAR(result.position.x, 5.0f, 0.05f);
}

TEST(CreatureChaseMovementTest, StepAlongNavMeshReplansWhenTargetRelocated) {
  MockCollisionQueries mock;
  mock.navMeshAvailable = true;
  application::combat::CreatureChaseConfig config{};
  config.runSpeedYardsPerSec = 10.0f;
  config.stopDistanceYards = 1.0f;

  application::combat::ChaseNavMeshState state;
  state.lastTargetX = 0.0f;
  state.lastTargetY = 0.0f;
  state.lastTargetZ = 0.0f;
  state.waypoints.push_back(Firelands::Vec3{0.0f, 0.0f, 0.0f});
  state.waypoints.push_back(Firelands::Vec3{5.0f, 0.0f, 0.0f});
  state.currentWaypoint = 1;

  auto const result = application::combat::StepCreatureAlongNavMeshPath(
      MakePos(0.0f, 0.0f, 0.0f), 50.0f, 0.0f, 0.0f, 0.5f, config, state, &mock, 0u);

  EXPECT_TRUE(result.moved);
  EXPECT_EQ(state.lastTargetX, 50.0f);
}
