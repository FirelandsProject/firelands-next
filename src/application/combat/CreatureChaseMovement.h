#pragma once

#include <shared/network/MovementInfo.h>
#include <application/ports/IMapCollisionQueries.h>
#include <vector>

namespace application::combat {

struct CreatureChaseConfig {
  float runSpeedYardsPerSec = 7.0f;
  float stopDistanceYards = 1.0f;
  float zBlendPerTick = 0.35f;
};

struct CreatureChaseStepResult {
  bool moved = false;
  bool inStopRange = false;
  Firelands::MovementInfo position{};
};

struct ChaseNavMeshState {
  std::vector<Firelands::Vec3> waypoints;
  size_t currentWaypoint = 0;
  float lastTargetX = 0.0f;
  float lastTargetY = 0.0f;
  float lastTargetZ = 0.0f;
};

Firelands::MovementInfo ComputeChaseStandPosition(Firelands::MovementInfo const &from,
                                       float targetX, float targetY, float targetZ,
                                       float stopDistanceYards);

CreatureChaseStepResult StepCreatureTowardTarget(Firelands::MovementInfo const &current,
                                                float targetX, float targetY,
                                                float targetZ, float deltaSeconds,
                                                CreatureChaseConfig const &config);

CreatureChaseStepResult ProjectCreatureTowardTarget(Firelands::MovementInfo const &current,
                                                  float targetX, float targetY,
                                                  float targetZ, float maxDeltaSeconds,
                                                  CreatureChaseConfig const &config);

bool ChaseTargetRelocated(float lastX, float lastY, float lastZ, float newX, float newY,
                          float newZ, float thresholdYards = 0.5f);

std::vector<Firelands::Vec3> ComputeNavMeshPath(
    uint32_t mapId,
    Firelands::MovementInfo const &start,
    float targetX, float targetY, float targetZ,
    Firelands::IMapCollisionQueries const *collision);

CreatureChaseStepResult StepCreatureAlongNavMeshPath(
    Firelands::MovementInfo const &current,
    float targetX, float targetY, float targetZ,
    float deltaSeconds,
    CreatureChaseConfig const &config,
    ChaseNavMeshState &state,
    Firelands::IMapCollisionQueries const *collision,
    uint32_t mapId);

} // namespace application::combat
