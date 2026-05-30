#include "CreatureChaseMovement.h"

#include <shared/network/MovementFlags.h>
#include <application/ports/IMapCollisionQueries.h>
#include <shared/Logger.h>
#include <cmath>

using Firelands::MOVEMENTFLAG_FORWARD;
using Firelands::MOVEMENTFLAG_NONE;
using Firelands::MovementInfo;
using Firelands::Vec3;

namespace application::combat {

namespace {

float DistanceSquared2d(float ax, float ay, float bx, float by) {
  float const dx = ax - bx;
  float const dy = ay - by;
  return dx * dx + dy * dy;
}

float OrientationToward(float fromX, float fromY, float toX, float toY) {
  return std::atan2(toY - fromY, toX - fromX);
}

} // namespace

MovementInfo ComputeChaseStandPosition(MovementInfo const &from, float targetX,
                                       float targetY, float targetZ,
                                       float stopDistanceYards) {
  float const dx = from.x - targetX;
  float const dy = from.y - targetY;
  float const distSq = dx * dx + dy * dy;

  MovementInfo pos = from;
  if (distSq < 1e-8f) {
    pos.orientation = OrientationToward(from.x, from.y, targetX, targetY);
    pos.flags = MOVEMENTFLAG_NONE;
    return pos;
  }

  float const dist = std::sqrt(distSq);
  float const standDist = std::min(dist, stopDistanceYards);
  float const nx = dx / dist;
  float const ny = dy / dist;

  pos.x = targetX + nx * standDist;
  pos.y = targetY + ny * standDist;
  pos.z = from.z + (targetZ - from.z) * (standDist / dist);
  pos.orientation = OrientationToward(pos.x, pos.y, targetX, targetY);
  pos.flags = MOVEMENTFLAG_NONE;
  return pos;
}

CreatureChaseStepResult StepCreatureTowardTarget(Firelands::MovementInfo const &current,
                                                float targetX, float targetY,
                                                float targetZ,
                                                float deltaSeconds,
                                                CreatureChaseConfig const &config) {
  CreatureChaseStepResult result{};
  result.position = current;

  float const stopSq = config.stopDistanceYards * config.stopDistanceYards;
  float const distSq =
      DistanceSquared2d(current.x, current.y, targetX, targetY);
  if (distSq <= stopSq) {
    result.inStopRange = true;
    result.position.orientation =
        OrientationToward(current.x, current.y, targetX, targetY);
    result.position.flags = MOVEMENTFLAG_NONE;
    result.moved = false;
    return result;
  }

  float const dist = std::sqrt(distSq);
  float const maxStep = std::max(0.01f, config.runSpeedYardsPerSec * deltaSeconds);
  float const allowed = std::max(0.f, dist - config.stopDistanceYards);
  float const step = std::min(maxStep, allowed);

  float const nx = current.x + (targetX - current.x) / dist * step;
  float const ny = current.y + (targetY - current.y) / dist * step;
  float const dz = targetZ - current.z;
  float const nz =
      std::abs(dz) > 0.01f ? current.z + dz * config.zBlendPerTick : current.z;

  result.position.x = nx;
  result.position.y = ny;
  result.position.z = nz;
  result.position.orientation = OrientationToward(nx, ny, targetX, targetY);
  result.position.flags = MOVEMENTFLAG_FORWARD;
  result.moved = step > 0.01f;
  result.inStopRange = false;
  return result;
}

CreatureChaseStepResult ProjectCreatureTowardTarget(MovementInfo const &current,
                                                  float targetX, float targetY,
                                                  float targetZ, float maxDeltaSeconds,
                                                  CreatureChaseConfig const &config) {
  constexpr float kSimTickSeconds = 0.2f;

  CreatureChaseStepResult result{};
  result.position = current;
  result.moved = false;
  result.inStopRange = false;

  float remaining = std::max(0.f, maxDeltaSeconds);
  while (remaining > 1e-4f) {
    float const slice = std::min(remaining, kSimTickSeconds);
    auto const step =
        StepCreatureTowardTarget(result.position, targetX, targetY, targetZ, slice, config);
    result = step;
    if (step.inStopRange || !step.moved)
      break;
    remaining -= slice;
  }
  return result;
}

bool ChaseTargetRelocated(float lastX, float lastY, float lastZ, float newX, float newY,
                          float newZ, float thresholdYards) {
  float const dx = newX - lastX;
  float const dy = newY - lastY;
  float const dz = newZ - lastZ;
  float const thresholdSq = thresholdYards * thresholdYards;
  return (dx * dx + dy * dy + dz * dz) > thresholdSq;
}

std::vector<Vec3> ComputeNavMeshPath(uint32_t mapId,
                                     MovementInfo const &start, float targetX,
                                     float targetY, float targetZ,
                                     Firelands::IMapCollisionQueries const *collision) {
  std::vector<Vec3> waypoints;
  if (!collision) {
    LOG_DEBUG("CHASE navmesh path skipped: mapId={} no collision service", mapId);
    return waypoints;
  }

  Firelands::FindPathRequest req;
  req.mapId = mapId;
  req.startX = start.x;
  req.startY = start.y;
  req.startZ = start.z;
  req.endX = targetX;
  req.endY = targetY;
  req.endZ = targetZ;
  req.smoothPath = true;
  req.allowPartialPath = true;

  auto result = collision->FindPath(req);
  if (result.status == Firelands::FindPathStatus::Complete ||
      result.status == Firelands::FindPathStatus::Partial) {
    waypoints = std::move(result.waypoints);
  } else {
    LOG_DEBUG("CHASE navmesh path failed: mapId={} status={} start=({}, {}, {}) end=({}, {}, {})",
              mapId, static_cast<int>(result.status), start.x, start.y, start.z,
              targetX, targetY, targetZ);
  }
  return waypoints;
}

CreatureChaseStepResult StepCreatureAlongNavMeshPath(
    MovementInfo const &current, float targetX, float targetY, float targetZ,
    float deltaSeconds, CreatureChaseConfig const &config,
    ChaseNavMeshState &state,
    Firelands::IMapCollisionQueries const *collision,
    uint32_t mapId) {
  bool const targetRelocated =
      ChaseTargetRelocated(state.lastTargetX, state.lastTargetY,
                           state.lastTargetZ, targetX, targetY, targetZ);

  if (targetRelocated || state.waypoints.empty()) {
    state.lastTargetX = targetX;
    state.lastTargetY = targetY;
    state.lastTargetZ = targetZ;
    state.currentWaypoint = 0;
    state.waypoints.clear();
  }

  if (targetRelocated && collision) {
    state.waypoints = ComputeNavMeshPath(mapId, current, targetX, targetY, targetZ, collision);
    if (!state.waypoints.empty()) {
      state.waypoints.push_back(Vec3{targetX, targetY, targetZ});
      state.currentWaypoint = 0;
      LOG_DEBUG("CHASE navmesh path ready: mapId={} waypointCount={} current=({}, {}, {}) target=({}, {}, {})",
                mapId, state.waypoints.size(), current.x, current.y, current.z,
                targetX, targetY, targetZ);
    } else {
      LOG_DEBUG("CHASE navmesh path empty: mapId={} current=({}, {}, {}) target=({}, {}, {}) fallback=straight-line",
                mapId, current.x, current.y, current.z, targetX, targetY, targetZ);
    }
  }

  if (state.waypoints.empty() || state.currentWaypoint >= state.waypoints.size()) {
    if (!collision) {
      LOG_DEBUG("CHASE fallback without collision: mapId={} current=({}, {}, {}) target=({}, {}, {})",
                mapId, current.x, current.y, current.z, targetX, targetY, targetZ);
    }
    return StepCreatureTowardTarget(current, targetX, targetY, targetZ,
                                    deltaSeconds, config);
  }

  float const directDx = targetX - current.x;
  float const directDy = targetY - current.y;
  float const directDistSq = directDx * directDx + directDy * directDy;

  while (state.currentWaypoint < state.waypoints.size()) {
    Vec3 const &wp = state.waypoints[state.currentWaypoint];
    float const dx = current.x - wp.x;
    float const dy = current.y - wp.y;
    float const distSq = dx * dx + dy * dy;

    if (distSq < 0.25f) {
      ++state.currentWaypoint;
      LOG_TRACE("CHASE waypoint reached: mapId={} waypointIndex={} pos=({}, {}, {})",
                mapId, state.currentWaypoint - 1, wp.x, wp.y, wp.z);
      continue;
    }

    // Skip a waypoint that goes the wrong way. Detour's findStraightPath can
    // emit a start-projection point far from the actual creature when
    // findNearestPoly picks a stale/corrupted poly; following it sends the NPC
    // *away* from the player. "Wrong way" = the vector from current to wp
    // points away from the target, AND wp is farther from the target than
    // current already is.
    float const wpToTargetX = targetX - wp.x;
    float const wpToTargetY = targetY - wp.y;
    float const wpToTargetDistSq =
        wpToTargetX * wpToTargetX + wpToTargetY * wpToTargetY;
    float const wpFromCurrentDot =
        (wp.x - current.x) * directDx + (wp.y - current.y) * directDy;
    if (wpFromCurrentDot < 0.f && wpToTargetDistSq > directDistSq) {
      LOG_DEBUG(
          "CHASE skipping wrong-way waypoint: mapId={} waypointIndex={} "
          "wp=({}, {}, {}) current=({}, {}, {}) target=({}, {})",
          mapId, state.currentWaypoint, wp.x, wp.y, wp.z, current.x, current.y,
          current.z, targetX, targetY);
      ++state.currentWaypoint;
      continue;
    }

    auto step = StepCreatureTowardTarget(current, wp.x, wp.y, wp.z, deltaSeconds, config);
    return step;
  }

  return StepCreatureTowardTarget(current, targetX, targetY, targetZ,
                                  deltaSeconds, config);
}

} // namespace application::combat
