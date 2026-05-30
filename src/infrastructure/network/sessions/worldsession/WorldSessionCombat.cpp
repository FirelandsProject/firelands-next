#include <application/combat/CombatEntityAdapters.h>
#include <application/combat/CombatHostility.h>
#include <application/combat/CombatService.h>
#include <application/combat/CreatureChaseMovement.h>
#include <application/ports/IMapCollisionQueries.h>
#include <application/services/WorldService.h>
#include <application/spell/SpellManager.h>
#include <algorithm>
#include <boost/asio/redirect_error.hpp>
#include <chrono>
#include <cmath>
#include <unordered_set>
#include <vector>
#include <domain/world/Creature.h>
#include <domain/world/Map.h>
#include <domain/world/Player.h>
#include <infrastructure/network/asio/AsioAwaitables.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionSpellEffects.h>
#include <shared/game/MeleeRange.h>
#include <shared/game/PlayerGmAppearance.h>
#include <shared/game/UnitFieldFlags.h>
#include <shared/network/SpellCastWire.h>
#include <shared/network/MonsterMovePackets.h>
#include <shared/network/MovementFlags.h>
#include <shared/network/WorldPacket.h>
#include <shared/network/packets/server/CombatPackets.h>

namespace Firelands {

namespace {

constexpr std::chrono::milliseconds kCreatureMoveTickInterval{200};
/// Long enough for one continuous client run cycle; replan when it finishes or target moves.
constexpr float kCreatureSplineHorizonSeconds = 1.5f;
/// Client starts spline playback after packet delivery; lag server position to match.
constexpr float kSplineClientPlaybackDelayMs = 150.f;
constexpr float kCreatureHomeArrivalYards = 0.75f;
constexpr float kCreatureChaseStandArrivalYards = 0.5f;
constexpr float kCreatureRunSpeedYardsPerSec = 7.0f;

/// Max distance from spawn/home before the creature abandons combat (evade).
constexpr float kCreatureLeashDistanceFromHomeYards = 50.0f;
/// Max distance to the chased player before the creature drops aggro.
constexpr float kCreatureMaxChaseTargetDistanceYards = 45.0f;
constexpr std::chrono::milliseconds kCreatureMeleeSwingInterval{2000};
constexpr std::chrono::milliseconds kCreatureSpellTryInterval{2500};

float DistanceSquared2d(float ax, float ay, float bx, float by) {
  float const dx = ax - bx;
  float const dy = ay - by;
  return dx * dx + dy * dy;
}

bool IsBeyondLeashFromHome(MovementInfo const &home, float x, float y) {
  float const leashSq = kCreatureLeashDistanceFromHomeYards * kCreatureLeashDistanceFromHomeYards;
  return DistanceSquared2d(home.x, home.y, x, y) > leashSq;
}

bool IsTargetTooFarToChase(float creatureX, float creatureY, float targetX,
                           float targetY) {
  float const maxSq =
      kCreatureMaxChaseTargetDistanceYards * kCreatureMaxChaseTargetDistanceYards;
  return DistanceSquared2d(creatureX, creatureY, targetX, targetY) > maxSq;
}

void SendMeleeSwingFailure(WorldSession &session, application::MeleeSwingResult result) {
  WorldPacket pkt;
  switch (result) {
  case application::MeleeSwingResult::DeadTarget:
    pkt = combat_wire::BuildAttackSwingDeadTarget();
    break;
  case application::MeleeSwingResult::CantAttack:
    pkt = combat_wire::BuildAttackSwingCantAttack();
    break;
  case application::MeleeSwingResult::NotInRange:
    pkt = combat_wire::BuildAttackSwingNotInRange();
    break;
  default:
    return;
  }
  session.SendPacket(pkt);
}

uint32_t HealthDamageDealt(uint32_t before, uint32_t after) {
  return before > after ? before - after : 0u;
}

void BroadcastMeleeHit(std::shared_ptr<Map> const &map, uint32 mapId, uint64 attackerGuid,
                       uint64 victimGuid, uint32 damage, uint32 victimHealth,
                       uint32 victimMaxHealth) {
  if (!map || damage == 0)
    return;
  WorldPacket state =
      combat_wire::BuildAttackerStateUpdate(attackerGuid, victimGuid, damage, victimHealth);
  map->BroadcastPacketToNearby(attackerGuid, state, true);
  BroadcastUnitHealthAfterDelta(mapId, map, victimGuid, victimHealth, victimMaxHealth);
}

void BroadcastCreatureUnitWireState(uint32 mapId, std::shared_ptr<Map> const &map,
                                    Creature const &creature) {
  if (!map)
    return;
  uint64 const guid = creature.GetGuid();
  BroadcastUnitFlagsOnMap(mapId, map, guid, creature.GetUnitFieldFlags());
  BroadcastUnitDynamicFlagsOnMap(mapId, map, guid, creature.GetUnitDynamicFlags());
}

bool SessionPlayerInMeleeRangeOf(WorldSession const &session, float targetX, float targetY,
                                 float targetZ) {
  MovementInfo const &pos = session.GetPosition();
  return IsWithinMeleeRange3d(pos.x, pos.y, pos.z, targetX, targetY, targetZ);
}

bool HasClearMeleeLineOfSight(uint32 mapId, float fromX, float fromY, float fromZ,
                              float toX, float toY, float toZ) {
  auto collision = WorldService::Instance().GetCollisionQueries();
  if (!collision)
    return true;
  bool const clear = collision->LineOfSight(mapId, fromX, fromY, fromZ, toX, toY, toZ);
  if (!clear) {
    static thread_local std::chrono::steady_clock::time_point lastLog{};
    auto const now = std::chrono::steady_clock::now();
    if (lastLog.time_since_epoch().count() == 0 ||
        now - lastLog >= std::chrono::seconds(2)) {
      lastLog = now;
      LOG_DEBUG("MELEE LoS blocked: mapId={} from=({}, {}, {}) to=({}, {}, {})",
                mapId, fromX, fromY, fromZ, toX, toY, toZ);
    }
  }
  return clear;
}

bool SessionPlayerInMeleeRangeOfNpc(WorldSession const &session, Creature const &creature) {
  MovementInfo const &pos = session.GetPosition();
  if (!IsWithinMeleeRangeAgainstNpc(pos.x, pos.y, pos.z, creature.GetX(), creature.GetY(),
                                    creature.GetZ())) {
    return false;
  }
  return HasClearMeleeLineOfSight(session.GetMapId(), creature.GetX(), creature.GetY(),
                                  creature.GetZ(), pos.x, pos.y, pos.z);
}

void BroadcastCreatureChaseMove(std::shared_ptr<Map> const &map, uint64 creatureGuid,
                                uint64_t faceTargetGuid, MovementInfo const &from,
                                MovementInfo const &to, int32_t splineId) {
  if (!map)
    return;
  uint32_t const durationMs = monster_move_wire::MonsterMoveDurationMs(
      from.x, from.y, from.z, to.x, to.y, to.z, kCreatureRunSpeedYardsPerSec);
  WorldPacket pkt = monster_move_wire::BuildMonsterMoveToPosition(
      creatureGuid, from.x, from.y, from.z, to.x, to.y, to.z, splineId, durationMs,
      monster_move_wire::FacingTarget, 0.f, faceTargetGuid);
  map->BroadcastPacketToNearby(creatureGuid, pkt, true);
}

void BroadcastCreatureReturnMove(std::shared_ptr<Map> const &map, uint64 creatureGuid,
                                 MovementInfo const &from, MovementInfo const &to,
                                 int32_t splineId) {
  if (!map)
    return;
  uint32_t const durationMs = monster_move_wire::MonsterMoveDurationMs(
      from.x, from.y, from.z, to.x, to.y, to.z, kCreatureRunSpeedYardsPerSec);
  // Face along the spline (step orientation), not the final home spot — avoids
  // snapping toward the player when they sit between the NPC and spawn.
  WorldPacket pkt = monster_move_wire::BuildMonsterMoveToPosition(
      creatureGuid, from.x, from.y, from.z, to.x, to.y, to.z, splineId, durationMs,
      monster_move_wire::FacingAngle, to.orientation);
  map->BroadcastPacketToNearby(creatureGuid, pkt, true);
}

void BroadcastCreatureMonsterStop(std::shared_ptr<Map> const &map,
                                  std::shared_ptr<Creature> const &creature,
                                  int32_t splineId) {
  if (!map || !creature)
    return;
  MovementInfo const &pos = creature->GetPosition();
  WorldPacket pkt = monster_move_wire::BuildMonsterMoveStop(creature->GetGuid(), pos.x, pos.y,
                                                            pos.z, splineId);
  map->BroadcastPacketToNearby(creature->GetGuid(), pkt, true);
}

using ActiveCreatureSpline = WorldSession::CreatureCombatRuntime::ActiveSpline;

float SplineElapsedMs(ActiveCreatureSpline const &spline,
                      std::chrono::steady_clock::time_point now,
                      bool clientAligned) {
  float const elapsedMs =
      std::chrono::duration<float, std::milli>(now - spline.startedAt).count();
  if (!clientAligned)
    return elapsedMs;
  return std::max(0.f, elapsedMs - kSplineClientPlaybackDelayMs);
}

bool IsCreatureSplineInFlight(WorldSession::CreatureCombatRuntime const &runtime,
                              std::chrono::steady_clock::time_point now,
                              bool clientAligned = true) {
  if (!runtime.activeSpline.has_value())
    return false;
  ActiveCreatureSpline const &spline = *runtime.activeSpline;
  return SplineElapsedMs(spline, now, clientAligned) <
         static_cast<float>(spline.durationMs);
}

MovementInfo InterpolateActiveSpline(ActiveCreatureSpline const &spline,
                                     std::chrono::steady_clock::time_point now,
                                     bool clientAligned = true) {
  float const elapsedMs = SplineElapsedMs(spline, now, clientAligned);
  float const t =
      spline.durationMs > 0
          ? std::clamp(elapsedMs / static_cast<float>(spline.durationMs), 0.f, 1.f)
          : 1.f;

  MovementInfo pos{};
  pos.x = spline.from.x + (spline.to.x - spline.from.x) * t;
  pos.y = spline.from.y + (spline.to.y - spline.from.y) * t;
  pos.z = spline.from.z + (spline.to.z - spline.from.z) * t;
  pos.orientation =
      spline.from.orientation + (spline.to.orientation - spline.from.orientation) * t;
  pos.flags = t >= 1.f ? MOVEMENTFLAG_NONE : MOVEMENTFLAG_FORWARD;
  return pos;
}

MovementInfo GetCreatureClientVisiblePosition(
    Creature const &creature, WorldSession::CreatureCombatRuntime const &runtime,
    std::chrono::steady_clock::time_point now) {
  if (runtime.activeSpline.has_value())
    return InterpolateActiveSpline(*runtime.activeSpline, now, true);
  return creature.GetPosition();
}

bool IsCreatureInMeleeRangeOfPlayer(Creature const &creature, WorldSession const &session,
                                    WorldSession::CreatureCombatRuntime const &runtime,
                                    std::chrono::steady_clock::time_point now) {
  MovementInfo const &playerPos = session.GetPosition();
  (void)runtime;
  (void)now;
  MovementInfo const &creaturePos = creature.GetPosition();
  if (!IsWithinMeleeRangeAgainstNpc(playerPos.x, playerPos.y, playerPos.z, creaturePos.x,
                                    creaturePos.y, creaturePos.z)) {
    return false;
  }
  return HasClearMeleeLineOfSight(session.GetMapId(), creaturePos.x, creaturePos.y,
                                  creaturePos.z, playerPos.x, playerPos.y, playerPos.z);
}

bool SessionPlayerInMeleeRangeOfNpc(WorldSession const &session, Creature const &creature,
                                    WorldSession::CreatureCombatRuntime const &runtime,
                                    std::chrono::steady_clock::time_point now) {
  (void)runtime;
  (void)now;
  return SessionPlayerInMeleeRangeOfNpc(session, creature);
}

/// Advances server position along the client-aligned spline timeline.
void SyncCreatureToActiveSpline(std::shared_ptr<Map> const &map,
                                std::shared_ptr<Creature> const &creature,
                                WorldSession::CreatureCombatRuntime &runtime,
                                std::chrono::steady_clock::time_point now) {
  if (!map || !creature || !runtime.activeSpline.has_value())
    return;

  ActiveCreatureSpline const &spline = *runtime.activeSpline;
  if (!IsCreatureSplineInFlight(runtime, now, true)) {
    MovementInfo const arrived = InterpolateActiveSpline(spline, now, true);
    map->UpdateObjectPosition(creature->GetGuid(), arrived);
    runtime.activeSpline.reset();
    return;
  }

  map->UpdateObjectPosition(creature->GetGuid(),
                            InterpolateActiveSpline(spline, now, true));
}

void StartCreatureActiveSpline(WorldSession::CreatureCombatRuntime &runtime,
                               MovementInfo const &from, MovementInfo const &to,
                               std::chrono::steady_clock::time_point now,
                               uint32_t durationMs) {
  ActiveCreatureSpline spline{};
  spline.from = from;
  spline.to = to;
  spline.startedAt = now;
  spline.durationMs = durationMs;
  runtime.activeSpline = spline;
}

void ClearCreatureSplineInFlight(WorldSession::CreatureCombatRuntime &runtime) {
  runtime.activeSpline.reset();
}

bool MovedMeaningfully(MovementInfo const &from, MovementInfo const &to) {
  float const dx = to.x - from.x;
  float const dy = to.y - from.y;
  float const dz = to.z - from.z;
  return (dx * dx + dy * dy + dz * dz) > 0.01f * 0.01f;
}

void SnapCreatureToChaseStand(std::shared_ptr<Map> const &map,
                              std::shared_ptr<Creature> const &creature,
                              MovementInfo const &stand) {
  if (!map || !creature)
    return;
  map->UpdateObjectPosition(creature->GetGuid(), stand);
}

bool TryFinalizeCreatureChaseStand(std::shared_ptr<Map> const &map,
                                   std::shared_ptr<Creature> const &creature,
                                   MovementInfo const &from, float targetX, float targetY,
                                   float targetZ, float stopDistanceYards,
                                   WorldSession::CreatureCombatRuntime &runtime,
                                   std::chrono::steady_clock::time_point now,
                                   uint32_t &moveCounter, uint64_t faceTargetGuid) {
  MovementInfo const stand =
      application::combat::ComputeChaseStandPosition(from, targetX, targetY, targetZ,
                                                     stopDistanceYards);
  float const standDistSq =
      DistanceSquared2d(from.x, from.y, stand.x, stand.y);

  LOG_MMAP_DEBUG(
      "CHASE finalize stand: mapId={} creatureGuid={} from=({}, {}, {}) "
      "target=({}, {}, {}) stand=({}, {}, {}) standDist2d={}",
      map ? map->GetMapId() : 0u, creature ? creature->GetGuid() : 0ULL, from.x,
      from.y, from.z, targetX, targetY, targetZ, stand.x, stand.y, stand.z,
      std::sqrt(standDistSq));

  if (IsCreatureSplineInFlight(runtime, now, true))
    return false;

  if (standDistSq > kCreatureChaseStandArrivalYards * kCreatureChaseStandArrivalYards) {
    int32_t const splineId = static_cast<int32_t>(++moveCounter);
    uint32_t const durationMs = monster_move_wire::MonsterMoveDurationMs(
        from.x, from.y, from.z, stand.x, stand.y, stand.z, kCreatureRunSpeedYardsPerSec);
    BroadcastCreatureChaseMove(map, creature->GetGuid(), faceTargetGuid, from, stand, splineId);
    StartCreatureActiveSpline(runtime, from, stand, now, durationMs);
    return false;
  }

  SnapCreatureToChaseStand(map, creature, stand);
  int32_t const stopId = static_cast<int32_t>(++moveCounter);
  BroadcastCreatureMonsterStop(map, creature, stopId);
  ClearCreatureSplineInFlight(runtime);
  MovementInfo chaseTarget{};
  chaseTarget.x = targetX;
  chaseTarget.y = targetY;
  chaseTarget.z = targetZ;
  runtime.lastChaseTargetPos = chaseTarget;
  return true;
}

/// Projects movement toward a target and, when the prior spline finished, sends one packet.
bool TryBroadcastCreatureSplineStep(std::shared_ptr<Map> const &map,
                                      std::shared_ptr<Creature> const &creature,
                                      float targetX, float targetY, float targetZ,
                                      float stopDistanceYards,
                                      WorldSession::CreatureCombatRuntime &runtime,
                                      std::chrono::steady_clock::time_point now,
                                      uint32_t &moveCounter, uint64_t faceTargetGuid,
                                      bool returnHomeSpline) {
  if (!map || !creature)
    return false;

  SyncCreatureToActiveSpline(map, creature, runtime, now);

  MovementInfo from = creature->GetPosition();
  auto const collisionQueries = WorldService::Instance().GetCollisionQueries();
  bool const useNavMesh =
      collisionQueries && collisionQueries->IsNavMeshDataAvailable(map->GetMapId());

  // Pin the chase target to the navmesh ground so creatures don't follow a
  // flying player into the air. Done before the relocation check so the
  // stored grounded Z and the new grounded Z compare consistently across
  // ticks (otherwise an airborne player would trigger a replan every tick).
  // Home splines already track a grounded point.
  float const targetZRaw = targetZ;
  if (collisionQueries && !returnHomeSpline) {
    float const projected = collisionQueries->GetHeight(map->GetMapId(),
                                                         targetX, targetY,
                                                         from.z);
    // If the navmesh ground is wildly different from the creature's current Z
    // (>15y), the .mmtile data is likely corrupted at this column. Fall back
    // to from.z so the creature keeps its current footing instead of being
    // teleported onto a ghost poly.
    constexpr float kMaxProjectionDriftYards = 15.0f;
    if (std::fabs(projected - from.z) > kMaxProjectionDriftYards) {
      LOG_MMAP_WARN(
          "CHASE rejecting navmesh target projection: mapId={} creatureGuid={} "
          "targetXY=({}, {}) navmeshZ={} fromZ={} delta={} -- using fromZ as "
          "fallback (likely corrupted .mmtile)",
          map->GetMapId(), creature->GetGuid(), targetX, targetY, projected,
          from.z, projected - from.z);
      targetZ = from.z;
    } else {
      targetZ = projected;
    }
    LOG_MMAP_DEBUG(
        "CHASE ground-project target: mapId={} creatureGuid={} from=({}, {}, {}) "
        "targetXY=({}, {}) targetZ_raw={} targetZ_grounded={} delta={}",
        map->GetMapId(), creature->GetGuid(), from.x, from.y, from.z, targetX,
        targetY, targetZRaw, targetZ, targetZ - targetZRaw);
  }

  bool const chaseTargetMoved =
      !returnHomeSpline &&
      (!runtime.lastChaseTargetPos.has_value() ||
       application::combat::ChaseTargetRelocated(runtime.lastChaseTargetPos->x,
                                                 runtime.lastChaseTargetPos->y,
                                                 runtime.lastChaseTargetPos->z, targetX,
                                                 targetY, targetZ));

  if (chaseTargetMoved && runtime.activeSpline.has_value()) {
    from = InterpolateActiveSpline(*runtime.activeSpline, now, true);
    map->UpdateObjectPosition(creature->GetGuid(), from);
    ClearCreatureSplineInFlight(runtime);
  }
  application::combat::CreatureChaseConfig config{};
  config.stopDistanceYards = stopDistanceYards;

  application::combat::CreatureChaseStepResult projected;
  if (useNavMesh && !returnHomeSpline) {
    projected = application::combat::StepCreatureAlongNavMeshPath(
        from, targetX, targetY, targetZ, kCreatureSplineHorizonSeconds, config,
        runtime.navMeshState, collisionQueries.get(), map->GetMapId());
  } else {
    projected = application::combat::ProjectCreatureTowardTarget(
        from, targetX, targetY, targetZ, kCreatureSplineHorizonSeconds, config);
  }

  // Belt-and-suspenders: even when the step skipped Z motion (stop-range case
  // or 2D-only waypoint), keep the broadcast position on the navmesh floor.
  if (collisionQueries && !returnHomeSpline) {
    float const preClampZ = projected.position.z;
    float groundZ = collisionQueries->GetHeight(
        map->GetMapId(), projected.position.x, projected.position.y,
        projected.position.z);
    // Safety net for corrupted navmesh tiles: if the resolved ground would
    // teleport the creature more than 5 yards vertically in a single tick,
    // ignore the navmesh value and keep the creature anchored at its current
    // Z. WoW terrain rarely changes more than 5y per step horizontally, so
    // this only kicks in when the .mmtile data is wrong.
    constexpr float kMaxVerticalStepYards = 5.0f;
    if (std::fabs(groundZ - from.z) > kMaxVerticalStepYards) {
      LOG_MMAP_WARN(
          "CHASE rejecting navmesh ground jump: mapId={} creatureGuid={} "
          "step=({}, {}, {}) navmeshZ={} fromZ={} delta={} -- using fromZ as "
          "fallback (likely corrupted .mmtile)",
          map->GetMapId(), creature->GetGuid(), projected.position.x,
          projected.position.y, projected.position.z, groundZ, from.z,
          groundZ - from.z);
      groundZ = from.z;
    }
    projected.position.z = groundZ;
    LOG_MMAP_DEBUG(
        "CHASE ground-clamp step: mapId={} creatureGuid={} step=({}, {}, {}) "
        "preClampZ={} clampedZ={} delta={} moved={} inStopRange={}",
        map->GetMapId(), creature->GetGuid(), projected.position.x,
        projected.position.y, projected.position.z, preClampZ,
        projected.position.z, projected.position.z - preClampZ, projected.moved,
        projected.inStopRange);
  }

  if (!returnHomeSpline) {
    float const distToPlayerSq =
        DistanceSquared2d(from.x, from.y, targetX, targetY);
    float const closeSq =
        (config.stopDistanceYards + kCreatureChaseStandArrivalYards) *
        (config.stopDistanceYards + kCreatureChaseStandArrivalYards);
    if (projected.inStopRange || distToPlayerSq <= closeSq) {
      return TryFinalizeCreatureChaseStand(map, creature, from, targetX, targetY, targetZ,
                                           stopDistanceYards, runtime, now, moveCounter,
                                           faceTargetGuid);
    }
  } else if (projected.inStopRange) {
    if (IsCreatureSplineInFlight(runtime, now, true))
      return false;

    SyncCreatureToActiveSpline(map, creature, runtime, now);
    int32_t const stopId = static_cast<int32_t>(++moveCounter);
    BroadcastCreatureMonsterStop(map, creature, stopId);
    ClearCreatureSplineInFlight(runtime);
    return true;
  }

  if (IsCreatureSplineInFlight(runtime, now, true) && !chaseTargetMoved)
    return false;

  if (!projected.moved || !MovedMeaningfully(from, projected.position))
    return false;

  MovementInfo const &to = projected.position;
  int32_t const splineId = static_cast<int32_t>(++moveCounter);
  uint32_t const durationMs = monster_move_wire::MonsterMoveDurationMs(
      from.x, from.y, from.z, to.x, to.y, to.z, kCreatureRunSpeedYardsPerSec);
  LOG_MMAP_DEBUG(
      "CHASE broadcast: mapId={} creatureGuid={} returnHome={} from=({}, {}, {}) "
      "to=({}, {}, {}) duration={}ms splineId={}",
      map->GetMapId(), creature->GetGuid(), returnHomeSpline, from.x, from.y,
      from.z, to.x, to.y, to.z, durationMs, splineId);
  if (returnHomeSpline)
    BroadcastCreatureReturnMove(map, creature->GetGuid(), from, to, splineId);
  else
    BroadcastCreatureChaseMove(map, creature->GetGuid(), faceTargetGuid, from, to, splineId);
  StartCreatureActiveSpline(runtime, from, to, now, durationMs);
  if (!returnHomeSpline) {
    MovementInfo chaseTarget{};
    chaseTarget.x = targetX;
    chaseTarget.y = targetY;
    chaseTarget.z = targetZ;
    runtime.lastChaseTargetPos = chaseTarget;
  }
  return false;
}

bool StepCreatureReturnToward(std::shared_ptr<Map> const &map,
                              std::shared_ptr<Creature> const &creature,
                              MovementInfo const &home,
                              WorldSession::CreatureCombatRuntime &runtime,
                              std::chrono::steady_clock::time_point now,
                              uint32_t &moveCounter) {
  return TryBroadcastCreatureSplineStep(map, creature, home.x, home.y, home.z,
                                        kCreatureHomeArrivalYards, runtime, now, moveCounter, 0,
                                        true);
}

bool FinalizeCreatureReturnAtHome(std::shared_ptr<Map> const &map, uint64_t creatureGuid,
                                  MovementInfo const &home, uint32_t &moveCounter,
                                  WorldSession::CreatureCombatRuntime &runtime,
                                  WorldSession *observer) {
  if (!map)
    return true;
  auto creature = map->TryGetCreature(creatureGuid);
  if (!creature)
    return true;

  SyncCreatureToActiveSpline(map, creature, runtime, std::chrono::steady_clock::now());
  ClearCreatureSplineInFlight(runtime);
  MovementInfo snapped = home;
  snapped.flags = MOVEMENTFLAG_NONE;
  map->UpdateObjectPosition(creatureGuid, snapped);
  creature->CompleteEvadeAtHome();
  BroadcastUnitHealthAfterDelta(map->GetMapId(), map, creatureGuid,
                                creature->GetLiveHealth(), creature->GetLiveMaxHealth(),
                                observer);
  BroadcastCreatureUnitWireState(map->GetMapId(), map, *creature);
  int32_t const stopId = static_cast<int32_t>(++moveCounter);
  BroadcastCreatureMonsterStop(map, creature, stopId);
  return true;
}

void ChaseCreatureTowardPlayer(std::shared_ptr<Map> const &map,
                               std::shared_ptr<Creature> const &creature,
                               std::shared_ptr<Player> const &target,
                               WorldSession const &session,
                               WorldSession::CreatureCombatRuntime &runtime,
                               std::chrono::steady_clock::time_point now) {
  if (!map || !creature || !target)
    return;

  MovementInfo const playerPos = session.GetPosition();
  map->UpdateObjectPosition(target->GetGuid(), playerPos);

  application::combat::CreatureChaseConfig config{};
  (void)TryBroadcastCreatureSplineStep(map, creature, playerPos.x, playerPos.y, playerPos.z,
                                       config.stopDistanceYards, runtime, now, runtime.moveCounter,
                                       target->GetGuid(), false);
}

} // namespace

void WorldSession::BeginCreatureReturnToHome(uint64_t creatureGuid,
                                           MovementInfo const &home,
                                           uint32_t initialMoveCounter) {
  if (_creatureReturningHome.find(creatureGuid) != _creatureReturningHome.end())
    return;

  auto map = runtime().GetMap(_mapId);
  if (!map)
    return;

  auto creature = map->TryGetCreature(creatureGuid);
  if (!creature || creature->GetLiveHealth() == 0)
    return;

  uint32_t moveCounter = initialMoveCounter;
  float const arriveSq = kCreatureHomeArrivalYards * kCreatureHomeArrivalYards;
  CreatureCombatRuntime scratch{};
  scratch.moveCounter = moveCounter;
  if (DistanceSquared2d(creature->GetX(), creature->GetY(), home.x, home.y) <= arriveSq) {
    (void)FinalizeCreatureReturnAtHome(map, creatureGuid, home, moveCounter, scratch, this);
    return;
  }

  auto const now = std::chrono::steady_clock::now();
  auto const emplaced = _creatureReturningHome.emplace(
      creatureGuid, CreatureCombatRuntime{home, moveCounter});
  CreatureCombatRuntime &returnRuntime = emplaced.first->second;
  uint32_t &returnCounter = returnRuntime.moveCounter;
  if (StepCreatureReturnToward(map, creature, home, returnRuntime, now, returnCounter)) {
    _creatureReturningHome.erase(creatureGuid);
    (void)FinalizeCreatureReturnAtHome(map, creatureGuid, home, returnCounter, returnRuntime,
                                     this);
    return;
  }
  ScheduleCreatureCombatMovement();
}

void WorldSession::StopCreatureAggro(uint64_t creatureGuid, bool sendAttackStopPackets) {
  auto it = _creatureAggroed.find(creatureGuid);
  if (it == _creatureAggroed.end())
    return;

  MovementInfo const home = it->second.home;
  uint32_t const moveCounter = it->second.moveCounter;
  CreatureCombatRuntime aggroRuntime = std::move(it->second);
  _creatureAggroed.erase(it);

  uint64_t const player = _playerGuid;

  if (auto map = runtime().GetMap(_mapId)) {
    if (auto creature = map->TryGetCreature(creatureGuid)) {
      SyncCreatureToActiveSpline(map, creature, aggroRuntime,
                                 std::chrono::steady_clock::now());
      ClearCreatureSplineInFlight(aggroRuntime);
      int32_t const stopId = static_cast<int32_t>(moveCounter + 1u);
      BroadcastCreatureMonsterStop(map, creature, stopId);
      if (creature->GetLiveHealth() > 0) {
        creature->ClearInCombat();
        creature->SetEvading(true);
        creature->SetChaseTargetPlayerGuid(0);
        BroadcastCreatureUnitWireState(_mapId, map, *creature);
      }
    }
    if (sendAttackStopPackets && player != 0) {
      WorldPacket stopCreature =
          combat_wire::BuildAttackStop(creatureGuid, player, false);
      map->BroadcastPacketToNearby(creatureGuid, stopCreature, true);
    }
    BroadcastUnitTargetOnMap(_mapId, map, creatureGuid, 0);
    BeginCreatureReturnToHome(creatureGuid, home, moveCounter);
    RefreshPlayerCombatWireState(map);
  }

  if (_creatureAggroed.empty() && _creatureReturningHome.empty())
    (void)_creatureCombatMoveTimer.cancel();
}

void WorldSession::StopAllCreatureCombat(bool sendAttackStopPackets) {
  if (auto map = runtime().GetMap(_mapId)) {
    for (auto const &entry : _creatureReturningHome) {
      if (auto creature = map->TryGetCreature(entry.first)) {
        creature->CompleteEvadeAtHome();
        BroadcastUnitHealthAfterDelta(_mapId, map, entry.first, creature->GetLiveHealth(),
                                      creature->GetLiveMaxHealth(), this);
      }
    }
  }
  std::vector<uint64_t> const aggroGuids = [&] {
    std::vector<uint64_t> guids;
    guids.reserve(_creatureAggroed.size());
    for (auto const &entry : _creatureAggroed)
      guids.push_back(entry.first);
    return guids;
  }();
  for (uint64_t const guid : aggroGuids)
    StopCreatureAggro(guid, sendAttackStopPackets);
  _creatureReturningHome.clear();
  (void)_creatureCombatMoveTimer.cancel();
}

void WorldSession::StartCreatureAggro(uint64_t creatureGuid) {
  if (_playerGuid == 0 || creatureGuid == 0 || !_combatService)
    return;

  _creatureReturningHome.erase(creatureGuid);

  auto map = runtime().GetMap(_mapId);
  if (!map)
    return;

  auto attackerPl = map->TryGetPlayer(_playerGuid);
  auto victimCr = map->TryGetCreature(creatureGuid);
  if (!attackerPl || !victimCr || attackerPl->GetLiveHealth() == 0 ||
      victimCr->GetLiveHealth() == 0) {
    return;
  }
  if (attackerPl->IsGmModeEnabled()) {
    LOG_DEBUG("GM aggro blocked: playerGuid={} creatureGuid={} mapId={}",
              _playerGuid, creatureGuid, _mapId);
    return;
  }
  if (!application::CanMeleeAttack(*attackerPl, *victimCr, _factionTemplateDbc.get()))
    return;

  bool const alreadyAggro = _creatureAggroed.find(creatureGuid) != _creatureAggroed.end();
  auto &runtime = _creatureAggroed[creatureGuid];
  auto const now = std::chrono::steady_clock::now();
  if (!alreadyAggro) {
    runtime.home = victimCr->GetPosition();
    runtime.moveCounter = 0;
    runtime.nextMeleeSwingAt = now;
    runtime.nextSpellTryAt = now + std::chrono::milliseconds(500);
    runtime.combatSpells.clear();
    runtime.nextSpellIndex = 0;
    runtime.spellCooldownUntil.clear();
    if (_npcTemplateSearch) {
      if (auto const tpl = _npcTemplateSearch->TryGetByEntry(victimCr->GetEntry()))
        runtime.combatSpells = tpl->combatSpells;
    }
  }

  application::adapters::CreatureCombatEntity creatureAttacker(victimCr);
  application::adapters::PlayerCombatEntity playerVictim(attackerPl);
  _combatService->StartCombat(creatureAttacker, playerVictim);

  victimCr->MarkInCombat();
  victimCr->SetEvading(false);
  victimCr->SetChaseTargetPlayerGuid(_playerGuid);
  EnterPlayerCombat();

  if (!alreadyAggro) {
    WorldPacket creatureStart = combat_wire::BuildAttackStart(creatureGuid, _playerGuid);
    SendPacket(creatureStart);
    map->BroadcastPacketToNearby(creatureGuid, creatureStart, true);
    BroadcastUnitTargetOnMap(_mapId, map, creatureGuid, _playerGuid);
    BroadcastCreatureUnitWireState(_mapId, map, *victimCr);
  }

  ScheduleCreatureCombatMovement();
}

void WorldSession::TryAggroCreatureFromSpellDamage(uint64_t targetGuid,
                                                   int32_t healthDelta) {
  if (healthDelta >= 0 || targetGuid == 0)
    return;
  StartCreatureAggro(targetGuid);
}

void WorldSession::StopMeleeAutoAttack(bool sendStopPackets) {
  (void)_meleeAutoAttackTimer.cancel();
  uint64_t const victim = _meleeVictimGuid;
  uint64_t const player = _playerGuid;
  bool const victimWasCreature = _meleeVictimIsCreature;

  _meleeVictimGuid = 0;
  _meleeVictimIsCreature = false;

  if (!sendStopPackets || player == 0 || victim == 0)
    return;

  auto map = runtime().GetMap(_mapId);
  if (!map)
    return;

  bool victimDead = false;
  if (victimWasCreature) {
    if (auto cr = map->TryGetCreature(victim))
      victimDead = cr->GetLiveHealth() == 0;
  } else if (auto pl = map->TryGetPlayer(victim)) {
    victimDead = pl->GetLiveHealth() == 0;
  }

  WorldPacket stopPlayer = combat_wire::BuildAttackStop(player, victim, victimDead);
  SendPacket(stopPlayer);
  map->BroadcastPacketToNearby(player, stopPlayer, false);
  BroadcastUnitTargetOnMap(_mapId, map, player, 0);
  RefreshPlayerCombatWireState(map);
}

void WorldSession::EvadeCreatureCombat(uint64_t creatureGuid) {
  StopCreatureAggro(creatureGuid, true);
  if (_meleeVictimGuid == creatureGuid)
    StopMeleeAutoAttack(true);
}

bool WorldSession::ShouldCreatureAbandonChase(std::shared_ptr<Map> const &map,
                                            std::shared_ptr<Creature> const &creature,
                                            std::shared_ptr<Player> const &target,
                                            MovementInfo const &home) const {
  if (!map || !creature || !target)
    return true;

  float const cx = creature->GetX();
  float const cy = creature->GetY();
  if (IsBeyondLeashFromHome(home, cx, cy))
    return true;
  if (IsTargetTooFarToChase(cx, cy, _position.x, _position.y))
    return true;
  return false;
}

void WorldSession::ProcessCreatureCombatAttack(std::shared_ptr<Map> const &map,
                                               std::shared_ptr<Creature> const &creature,
                                               std::shared_ptr<Player> const &target,
                                               CreatureCombatRuntime &runtime) {
  if (!map || !creature || !target || !_combatService)
    return;
  if (creature->GetLiveHealth() == 0 || target->GetLiveHealth() == 0)
    return;

  auto const now = std::chrono::steady_clock::now();

  if (!runtime.combatSpells.empty() && now >= runtime.nextSpellTryAt) {
    runtime.nextSpellTryAt = now + kCreatureSpellTryInterval;
    size_t const spellCount = runtime.combatSpells.size();
    for (size_t attempt = 0; attempt < spellCount; ++attempt) {
      uint32_t const spellId =
          runtime.combatSpells[runtime.nextSpellIndex % spellCount];
      runtime.nextSpellIndex = (runtime.nextSpellIndex + 1) % spellCount;
      if (TryCastCreatureCombatSpell(map, creature, target, runtime, spellId))
        return;
    }
  }

  if (now < runtime.nextMeleeSwingAt)
    return;
  if (!IsCreatureInMeleeRangeOfPlayer(*creature, *this, runtime, now))
    return;

  application::adapters::CreatureCombatEntity creatureAttacker(creature);
  application::adapters::PlayerCombatEntity playerVictim(target);
  uint32_t const playerHpBefore = target->GetLiveHealth();
  if (_combatService->ApplyMeleeHit(creatureAttacker, playerVictim) !=
      application::MeleeSwingResult::Success) {
    return;
  }

  runtime.nextMeleeSwingAt = now + kCreatureMeleeSwingInterval;
  uint32_t const playerDmg = HealthDamageDealt(playerHpBefore, target->GetLiveHealth());
  EnterPlayerCombat();
  BroadcastMeleeHit(map, _mapId, creature->GetGuid(), _playerGuid, playerDmg,
                    target->GetLiveHealth(), target->GetLiveMaxHealth());
}

bool WorldSession::TryCastCreatureCombatSpell(std::shared_ptr<Map> const &map,
                                              std::shared_ptr<Creature> const &creature,
                                              std::shared_ptr<Player> const &target,
                                              CreatureCombatRuntime &runtime,
                                              uint32_t spellId) {
  if (!map || !creature || !target || !_spellManager || spellId == 0)
    return false;

  auto const it = runtime.spellCooldownUntil.find(spellId);
  auto const now = std::chrono::steady_clock::now();
  if (it != runtime.spellCooldownUntil.end() && now < it->second)
    return false;

  static uint8_t s_creatureCastId = 0;
  uint8_t const castId = ++s_creatureCastId;

  std::unordered_set<uint32> knownSpells{spellId};
  SpellCastRequest req{};
  req.casterGuid = creature->GetGuid();
  req.mapId = _mapId;
  req.client.spellId = static_cast<int32_t>(spellId);
  req.client.castId = castId;
  req.client.targetFlags = SpellCastWire::TARGET_FLAG_UNIT;
  req.client.unitTargetGuid = _playerGuid;
  req.now = now;
  req.gcdReady = now;
  req.knownSpells = &knownSpells;
  req.casterLevel = creature->GetLevel() > 0 ? creature->GetLevel() : 1;
  req.hasCasterWorldPosition = true;
  req.casterX = creature->GetX();
  req.casterY = creature->GetY();
  req.casterZ = creature->GetZ();
  req.hasTargetWorldPosition = true;
  req.targetX = _position.x;
  req.targetY = _position.y;
  req.targetZ = _position.z;
  req.skipLineOfSight = true;
  req.spellCooldownUntilBySpellId = &runtime.spellCooldownUntil;

  if (std::shared_ptr<IMapCollisionQueries> collisionHeld =
          this->runtime().GetCollisionQueries())
    req.collisionQueries = collisionHeld.get();

  SpellCastOutcome out{};
  _spellManager->ProcessCastRequest(req, &out);
  if (out.kind != SpellCastOutcome::Kind::SpellStartAndGo)
    return false;

  map->BroadcastPacketToNearby(creature->GetGuid(), out.spellStart, true);
  map->BroadcastPacketToNearby(creature->GetGuid(), out.spellGo, true);
  ApplySpellCastAuraOnMap(_mapId, map, out, now);
  (void)ApplySpellCastOutcomeOnMap(_mapId, map, creature->GetGuid(), spellId, out, now);
  if (out.hasDirectHealthEffect && out.directHealthDelta < 0 &&
      out.directHealthTargetGuid == _playerGuid)
    EnterPlayerCombat();
  ScheduleSpellImpactVisual(map, creature->GetGuid(), spellId, out.primaryHitTargetGuid,
                            out.spellImpactDelayMs);

  if (out.spellCooldownDurationMs > 0)
    runtime.spellCooldownUntil[spellId] =
        now + std::chrono::milliseconds(static_cast<int64_t>(out.spellCooldownDurationMs));
  if (out.spellCategoryCooldownGroup > 0 && out.spellCategoryCooldownDurationMs > 0) {
    // Category cooldowns share the per-creature spell map keyed by group for AI simplicity.
    runtime.spellCooldownUntil[out.spellCategoryCooldownGroup] =
        now + std::chrono::milliseconds(
                  static_cast<int64_t>(out.spellCategoryCooldownDurationMs));
  }

  return true;
}

void WorldSession::SyncAggroedCreatureSplinePosition(uint64_t creatureGuid) {
  auto it = _creatureAggroed.find(creatureGuid);
  if (it == _creatureAggroed.end())
    return;

  auto map = runtime().GetMap(_mapId);
  if (!map)
    return;

  auto creature = map->TryGetCreature(creatureGuid);
  if (!creature)
    return;

  auto const now = std::chrono::steady_clock::now();
  SyncCreatureToActiveSpline(map, creature, it->second, now);
}

void WorldSession::ProcessCreatureCombatMovementTick() {
  if (_playerGuid == 0)
    return;

  auto map = runtime().GetMap(_mapId);
  if (!map) {
    StopAllCreatureCombat(false);
    StopMeleeAutoAttack(false);
    return;
  }

  auto attackerPl = map->TryGetPlayer(_playerGuid);
  if (!attackerPl) {
    StopAllCreatureCombat(false);
    return;
  }
  if (attackerPl->IsGmModeEnabled()) {
    if (!_creatureAggroed.empty() || !_creatureReturningHome.empty()) {
      LOG_DEBUG("GM combat cleanup: playerGuid={} mapId={} aggroed={} returningHome={}",
                _playerGuid, _mapId, _creatureAggroed.size(), _creatureReturningHome.size());
    }
    StopAllCreatureCombat(false);
    StopMeleeAutoAttack(false);
    return;
  }

  auto const now = std::chrono::steady_clock::now();
  for (auto returnIt = _creatureReturningHome.begin();
       returnIt != _creatureReturningHome.end();) {
    uint64_t const creatureGuid = returnIt->first;
    MovementInfo const home = returnIt->second.home;
    CreatureCombatRuntime &returnRuntime = returnIt->second;
    uint32_t &returnCounter = returnRuntime.moveCounter;
    auto creature = map->TryGetCreature(creatureGuid);
    if (!creature || creature->GetLiveHealth() == 0) {
      returnIt = _creatureReturningHome.erase(returnIt);
      continue;
    }

    float const arriveSq = kCreatureHomeArrivalYards * kCreatureHomeArrivalYards;
    if (DistanceSquared2d(creature->GetX(), creature->GetY(), home.x, home.y) <= arriveSq &&
        !IsCreatureSplineInFlight(returnRuntime, now, true)) {
      (void)FinalizeCreatureReturnAtHome(map, creatureGuid, home, returnCounter, returnRuntime,
                                         this);
      returnIt = _creatureReturningHome.erase(returnIt);
      continue;
    }

    uint32_t const hpBefore = creature->GetLiveHealth();
    creature->TickEvadeHealthRegen(kCreatureMoveTickInterval);
    if (creature->GetLiveHealth() != hpBefore) {
      BroadcastUnitHealthAfterDelta(_mapId, map, creatureGuid, creature->GetLiveHealth(),
                                    creature->GetLiveMaxHealth(), this);
    }

    if (StepCreatureReturnToward(map, creature, home, returnRuntime, now, returnCounter)) {
      (void)FinalizeCreatureReturnAtHome(map, creatureGuid, home, returnCounter, returnRuntime,
                                         this);
      returnIt = _creatureReturningHome.erase(returnIt);
      continue;
    }
    ++returnIt;
  }

  std::vector<uint64_t> evaded;
  for (auto &[creatureGuid, runtime] : _creatureAggroed) {
    auto creature = map->TryGetCreature(creatureGuid);
    if (!creature || creature->GetLiveHealth() == 0 || attackerPl->GetLiveHealth() == 0) {
      evaded.push_back(creatureGuid);
      continue;
    }
    if (ShouldCreatureAbandonChase(map, creature, attackerPl, runtime.home)) {
      evaded.push_back(creatureGuid);
      continue;
    }
    ChaseCreatureTowardPlayer(map, creature, attackerPl, *this, runtime, now);
    ProcessCreatureCombatAttack(map, creature, attackerPl, runtime);
  }

  for (uint64_t const guid : evaded) {
    StopCreatureAggro(guid, true);
    if (guid == _meleeVictimGuid)
      StopMeleeAutoAttack(true);
    SendNotification("Creature has evaded.");
  }

  if (!_creatureAggroed.empty() || !_creatureReturningHome.empty())
    ScheduleCreatureCombatMovement();
}

void WorldSession::ScheduleCreatureCombatMovement() {
  if (_creatureAggroed.empty() && _creatureReturningHome.empty())
    return;

  (void)_creatureCombatMoveTimer.cancel();
  _creatureCombatMoveTimer.expires_after(kCreatureMoveTickInterval);

  auto self = shared_from_this();
  Asio::SpawnDetached(_socket.get_executor(), [self, this]() -> Asio::awaitable<void> {
    boost::system::error_code ec;
    co_await _creatureCombatMoveTimer.async_wait(
        boost::asio::redirect_error(Asio::use_awaitable, ec));
    if (!ec)
      ProcessCreatureCombatMovementTick();
  });
}

void WorldSession::ProcessMeleeAutoAttackTick() {
  if (_playerGuid == 0 || _meleeVictimGuid == 0 || !_combatService)
    return;

  auto map = runtime().GetMap(_mapId);
  if (!map) {
    StopMeleeAutoAttack(false);
    return;
  }

  auto attackerPl = map->TryGetPlayer(_playerGuid);
  if (!attackerPl || attackerPl->GetLiveHealth() == 0) {
    StopMeleeAutoAttack(true);
    return;
  }

  uint64_t const victimGuid = _meleeVictimGuid;

  application::adapters::PlayerCombatEntity attackerEntity(attackerPl);

  if (_meleeVictimIsCreature) {
    auto victimCr = map->TryGetCreature(victimGuid);
    if (!victimCr) {
      StopMeleeAutoAttack(true);
      return;
    }
    SyncAggroedCreatureSplinePosition(victimGuid);
    auto const now = std::chrono::steady_clock::now();
    auto aggroIt = _creatureAggroed.find(victimGuid);
    bool const inRange =
        aggroIt != _creatureAggroed.end()
            ? SessionPlayerInMeleeRangeOfNpc(*this, *victimCr, aggroIt->second, now)
            : SessionPlayerInMeleeRangeOfNpc(*this, *victimCr);
    if (!inRange) {
      WorldPacket pkt = combat_wire::BuildAttackSwingNotInRange();
      SendPacket(pkt);
      ScheduleMeleeAutoAttack();
      return;
    }
    if (victimCr->GetLiveHealth() == 0) {
      StopMeleeAutoAttack(true);
      return;
    }

    application::adapters::CreatureCombatEntity victimEntity(victimCr);
    uint32_t const hpBefore = victimCr->GetLiveHealth();
    auto const hit = _combatService->ApplyMeleeHit(attackerEntity, victimEntity);
    if (hit != application::MeleeSwingResult::Success) {
      SendMeleeSwingFailure(*this, hit);
      if (hit == application::MeleeSwingResult::NotInRange)
        ScheduleMeleeAutoAttack();
      else
        StopMeleeAutoAttack(true);
      return;
    }
    uint32_t const dmg = HealthDamageDealt(hpBefore, victimCr->GetLiveHealth());
    BroadcastMeleeHit(map, _mapId, _playerGuid, victimGuid, dmg, victimCr->GetLiveHealth(),
                        victimCr->GetLiveMaxHealth());

    if (victimCr->GetLiveHealth() == 0) {
      FinalizeCreatureDeath(victimGuid, hpBefore);
      StopMeleeAutoAttack(true);
    }
    else
      ScheduleMeleeAutoAttack();
    return;
  }

  auto victimPl = map->TryGetPlayer(victimGuid);
  if (!victimPl) {
    StopMeleeAutoAttack(true);
    return;
  }
  if (!SessionPlayerInMeleeRangeOf(*this, victimPl->GetX(), victimPl->GetY(),
                                   victimPl->GetZ())) {
    WorldPacket pkt = combat_wire::BuildAttackSwingNotInRange();
    SendPacket(pkt);
    StopMeleeAutoAttack(true);
    return;
  }
  if (victimPl->GetLiveHealth() == 0) {
    StopMeleeAutoAttack(true);
    return;
  }

  application::adapters::PlayerCombatEntity victimEntity(victimPl);
  uint32_t const hpBefore = victimPl->GetLiveHealth();
  auto const hit = _combatService->ApplyMeleeHit(attackerEntity, victimEntity);
  if (hit != application::MeleeSwingResult::Success) {
    SendMeleeSwingFailure(*this, hit);
    StopMeleeAutoAttack(true);
    return;
  }
  uint32_t const dmg = HealthDamageDealt(hpBefore, victimPl->GetLiveHealth());
  BroadcastMeleeHit(map, _mapId, _playerGuid, victimGuid, dmg, victimPl->GetLiveHealth(),
                    victimPl->GetLiveMaxHealth());
  if (victimPl->GetLiveHealth() == 0)
    StopMeleeAutoAttack(true);
  else
    ScheduleMeleeAutoAttack();
}

void WorldSession::ScheduleMeleeAutoAttack() {
  if (_playerGuid == 0 || _meleeVictimGuid == 0)
    return;

  (void)_meleeAutoAttackTimer.cancel();
  _meleeAutoAttackTimer.expires_after(
      std::chrono::milliseconds(static_cast<int64_t>(kDefaultMainhandSwingMs)));

  auto self = shared_from_this();
  Asio::SpawnDetached(_socket.get_executor(), [self, this]() -> Asio::awaitable<void> {
    boost::system::error_code ec;
    co_await _meleeAutoAttackTimer.async_wait(
        boost::asio::redirect_error(Asio::use_awaitable, ec));
    if (!ec)
      ProcessMeleeAutoAttackTick();
  });
}

void WorldSession::HandleAttackStop(WorldPacket & /*packet*/) {
  StopMeleeAutoAttack(true);
}

bool WorldSession::PrepareMeleeRetarget(uint64_t victimGuid) {
  if (victimGuid != 0 && _meleeVictimGuid == victimGuid)
    return false;
  if (_meleeVictimGuid != 0)
    StopMeleeAutoAttack(true);
  return true;
}

void WorldSession::HandleAttackSwing(WorldPacket &packet) {
  if (_playerGuid == 0 || !_combatService)
    return;

  uint64_t const victimGuid = WorldSessionObjectUpdate::ReadClientTargetGuid(packet);
  if (victimGuid == 0 || victimGuid == _playerGuid)
    return;

  auto map = runtime().GetMap(_mapId);
  if (!map)
    return;

  auto attackerPl = map->TryGetPlayer(_playerGuid);
  if (!attackerPl || attackerPl->GetLiveHealth() == 0)
    return;

  application::adapters::PlayerCombatEntity attackerEntity(attackerPl);

  if (auto victimPl = map->TryGetPlayer(victimGuid)) {
    if (!SessionPlayerInMeleeRangeOf(*this, victimPl->GetX(), victimPl->GetY(),
                                     victimPl->GetZ())) {
      WorldPacket pkt = combat_wire::BuildAttackSwingNotInRange();
      SendPacket(pkt);
      return;
    }
    if (victimPl->GetLiveHealth() == 0) {
      WorldPacket pkt = combat_wire::BuildAttackSwingDeadTarget();
      SendPacket(pkt);
      return;
    }
    if (!application::CanMeleeAttack(*attackerPl, *victimPl)) {
      WorldPacket pkt = combat_wire::BuildAttackSwingCantAttack();
      SendPacket(pkt);
      return;
    }
    if (!PrepareMeleeRetarget(victimGuid))
      return;

    application::adapters::PlayerCombatEntity victimEntity(victimPl);
    uint32_t const hpBefore = victimPl->GetLiveHealth();
    auto const result = _combatService->BeginMeleeSwing(attackerEntity, victimEntity);
    if (result != application::MeleeSwingResult::Success) {
      SendMeleeSwingFailure(*this, result);
      return;
    }

    _meleeVictimGuid = victimGuid;
    _meleeVictimIsCreature = false;

    WorldPacket attackStart = combat_wire::BuildAttackStart(_playerGuid, victimGuid);
    SendPacket(attackStart);
    EnterPlayerCombat();
    BroadcastUnitTargetOnMap(_mapId, map, _playerGuid, victimGuid);

    uint32_t const dmg = HealthDamageDealt(hpBefore, victimPl->GetLiveHealth());
    BroadcastMeleeHit(map, _mapId, _playerGuid, victimGuid, dmg, victimPl->GetLiveHealth(),
                      victimPl->GetLiveMaxHealth());
    ScheduleMeleeAutoAttack();
    return;
  }

  if (auto victimCr = map->TryGetCreature(victimGuid)) {
    SyncAggroedCreatureSplinePosition(victimGuid);
    auto const now = std::chrono::steady_clock::now();
    auto aggroIt = _creatureAggroed.find(victimGuid);
    bool const inRange =
        aggroIt != _creatureAggroed.end()
            ? SessionPlayerInMeleeRangeOfNpc(*this, *victimCr, aggroIt->second, now)
            : SessionPlayerInMeleeRangeOfNpc(*this, *victimCr);
    if (!inRange) {
      WorldPacket pkt = combat_wire::BuildAttackSwingNotInRange();
      SendPacket(pkt);
      return;
    }
    if (victimCr->GetLiveHealth() == 0) {
      WorldPacket pkt = combat_wire::BuildAttackSwingDeadTarget();
      SendPacket(pkt);
      return;
    }
    if (!application::CanMeleeAttack(*attackerPl, *victimCr,
                                     _factionTemplateDbc.get())) {
      WorldPacket pkt = combat_wire::BuildAttackSwingCantAttack();
      SendPacket(pkt);
      return;
    }
    if (!PrepareMeleeRetarget(victimGuid))
      return;

    application::adapters::CreatureCombatEntity victimEntity(victimCr);
    uint32_t const hpBefore = victimCr->GetLiveHealth();
    auto const result = _combatService->BeginMeleeSwing(attackerEntity, victimEntity);
    if (result != application::MeleeSwingResult::Success) {
      SendMeleeSwingFailure(*this, result);
      return;
    }

    _meleeVictimGuid = victimGuid;
    _meleeVictimIsCreature = true;

    WorldPacket attackStart = combat_wire::BuildAttackStart(_playerGuid, victimGuid);
    SendPacket(attackStart);
    map->BroadcastPacketToNearby(_playerGuid, attackStart, false);
    EnterPlayerCombat();
    BroadcastUnitTargetOnMap(_mapId, map, _playerGuid, victimGuid);

    uint32_t const dmg = HealthDamageDealt(hpBefore, victimCr->GetLiveHealth());
    BroadcastMeleeHit(map, _mapId, _playerGuid, victimGuid, dmg, victimCr->GetLiveHealth(),
                      victimCr->GetLiveMaxHealth());
    if (victimCr->GetLiveHealth() == 0)
      FinalizeCreatureDeath(victimGuid, hpBefore);
    else
      StartCreatureAggro(victimGuid);

    ScheduleMeleeAutoAttack();
    return;
  }

  WorldPacket pkt = combat_wire::BuildAttackSwingCantAttack();
  SendPacket(pkt);
}

bool WorldSession::PlayerShouldShowInCombatOnWire() const {
  if (_meleeVictimGuid != 0)
    return true;
  if (!_creatureAggroed.empty())
    return true;
  if (auto map = runtime().GetMap(_mapId)) {
    if (auto pl = map->TryGetPlayer(_playerGuid)) {
      if (!pl->IsOutOfCombatForRegen(std::chrono::steady_clock::now()))
        return true;
    }
  }
  return false;
}

void WorldSession::EnterPlayerCombat() {
  auto map = runtime().GetMap(_mapId);
  if (!map || _playerGuid == 0)
    return;
  auto const now = std::chrono::steady_clock::now();
  if (auto pl = map->TryGetPlayer(_playerGuid))
    pl->MarkInCombat(now);
  RefreshPlayerCombatWireState(map);
}

void WorldSession::RefreshPlayerCombatWireState(std::shared_ptr<Map> const &map) {
  if (!map || _playerGuid == 0)
    return;
  bool const inCombat = PlayerShouldShowInCombatOnWire();
  uint32 const flags =
      ComputePlayerWireUnitFieldFlags(GetGmAppearanceForPlayerUpdates(), inCombat);
  WorldPacket pkt;
  WorldSessionObjectUpdate::BuildUnitFlagsValuesUpdate(static_cast<uint16>(_mapId),
                                                       _playerGuid, flags, pkt);
  SendPacket(pkt);
  map->BroadcastPacketToNearby(_playerGuid, pkt, true);
}

void WorldSession::FinalizeCreatureDeath(uint64 creatureGuid, uint32 hpBefore) {
  if (_playerGuid == 0 || creatureGuid == 0)
    return;
  auto map = runtime().GetMap(_mapId);
  if (!map)
    return;
  auto creature = map->TryGetCreature(creatureGuid);
  if (!creature)
    return;

  creature->MarkDeadAndLootable();
  creature->SetChaseTargetPlayerGuid(0);
  BroadcastUnitTargetOnMap(_mapId, map, creatureGuid, 0);
  BroadcastUnitHealthAfterDelta(_mapId, map, creatureGuid, creature->GetLiveHealth(),
                                creature->GetLiveMaxHealth(), this);
  BroadcastCreatureUnitWireState(_mapId, map, *creature);

  if (_creatureAggroed.find(creatureGuid) != _creatureAggroed.end())
    StopCreatureAggro(creatureGuid, true);
  else if (_meleeVictimGuid == creatureGuid)
    StopMeleeAutoAttack(true);

  MaybeGrantKillExperience(*creature, hpBefore);
}

} // namespace Firelands
