#pragma once

#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Firelands::monster_move_wire {

/// Matches `Movement::MonsterMoveType` in firelands-cata-ref `MovementTypedefs.h`.
enum MonsterMoveType : uint8_t {
  Normal = 0,
  Stop = 1,
  FacingSpot = 2,
  FacingTarget = 3,
  FacingAngle = 4,
};

/// Subset of `Movement::MoveSplineFlag` written on `SMSG_ON_MONSTER_MOVE` (see ref
/// `MoveSplineFlag.h`). `Mask_No_Monster_Move` strips facing/done/animation bits.
enum MoveSplineFlags : uint32_t {
  Done = 0x00000020u,
  SmoothGroundPath = 0x00100000u,
};

inline void AppendPositionXyz(WorldPacket &pkt, float x, float y, float z) {
  pkt.Append<float>(x);
  pkt.Append<float>(y);
  pkt.Append<float>(z);
}

/// Serializes `Movement::MovementSpline` (ref `MovementPackets.cpp` operator<<).
inline void AppendMovementSpline(WorldPacket &pkt, uint8_t face, float faceAngle,
                                 uint64_t faceTargetGuid, float faceSpotX, float faceSpotY,
                                 float faceSpotZ, uint32_t flags, uint32_t moveTimeMs,
                                 float destX, float destY, float destZ) {
  pkt.Append<int8_t>(static_cast<int8_t>(face));
  switch (face) {
  case FacingSpot:
    AppendPositionXyz(pkt, faceSpotX, faceSpotY, faceSpotZ);
    break;
  case FacingTarget:
    // Ref `operator<<(ObjectGuid)` on spline FaceGUID — full uint64, not packed.
    pkt.Append<uint64_t>(faceTargetGuid);
    break;
  case FacingAngle:
    pkt.Append<float>(faceAngle);
    break;
  default:
    break;
  }

  pkt.Append<int32_t>(static_cast<int32_t>(flags));
  pkt.Append<int32_t>(static_cast<int32_t>(moveTimeMs));

  int32_t const pointCount = 1;
  pkt.Append<int32_t>(pointCount);
  AppendPositionXyz(pkt, destX, destY, destZ);
}

/// Serializes `Movement::MovementMonsterSpline` (id + move).
inline void AppendMovementMonsterSpline(WorldPacket &pkt, int32_t splineId, uint8_t face,
                                        float faceAngle, uint64_t faceTargetGuid,
                                        float faceSpotX, float faceSpotY, float faceSpotZ,
                                        uint32_t flags, uint32_t moveTimeMs, float destX,
                                        float destY, float destZ) {
  pkt.Append<int32_t>(splineId);
  AppendMovementSpline(pkt, face, faceAngle, faceTargetGuid, faceSpotX, faceSpotY, faceSpotZ,
                       flags, moveTimeMs, destX, destY, destZ);
}

/// Move time from path length and run speed (ref `MoveSpline::Duration()`).
inline uint32_t MonsterMoveDurationMs(float fromX, float fromY, float fromZ, float toX,
                                      float toY, float toZ, float speedYardsPerSec) {
  float const dx = toX - fromX;
  float const dy = toY - fromY;
  float const dz = toZ - fromZ;
  float const dist = std::sqrt(dx * dx + dy * dy + dz * dz);
  float const speed = std::max(0.01f, speedYardsPerSec);
  float const ms = dist / speed * 1000.0f;
  return static_cast<uint32_t>(std::clamp(ms, 200.0f, 3000.0f));
}

/// Cataclysm 4.3.4.15595 `SMSG_ON_MONSTER_MOVE` — ref `WorldPackets::Movement::MonsterMove::Write`.
inline WorldPacket BuildMonsterMoveToPosition(
    uint64_t moverGuid, float posX, float posY, float posZ, float destX, float destY,
    float destZ, int32_t splineId, uint32_t durationMs, MonsterMoveType face = FacingAngle,
    float faceAngle = 0.f, uint64_t faceTargetGuid = 0, float faceSpotX = 0.f,
    float faceSpotY = 0.f, float faceSpotZ = 0.f, int8_t vehicleExitVoluntary = 0) {
  durationMs = std::max<uint32_t>(200u, durationMs);

  WorldPacket pkt(SMSG_ON_MONSTER_MOVE, 128);
  pkt.WritePackedGuid(moverGuid);
  pkt.Append<int8_t>(vehicleExitVoluntary);
  AppendPositionXyz(pkt, posX, posY, posZ);
  AppendMovementMonsterSpline(pkt, splineId, static_cast<uint8_t>(face), faceAngle,
                              faceTargetGuid, faceSpotX, faceSpotY, faceSpotZ, SmoothGroundPath,
                              durationMs, destX, destY, destZ);
  return pkt;
}

/// Stops an in-flight monster spline (ref `MoveSplineInit::Stop`, alive unit).
/// When `Face` is `Stop`, the client ends the spline block immediately (no flags/time/points).
inline WorldPacket BuildMonsterMoveStop(uint64_t moverGuid, float posX, float posY,
                                        float posZ, int32_t splineId,
                                        int8_t vehicleExitVoluntary = 0) {
  WorldPacket pkt(SMSG_ON_MONSTER_MOVE, 64);
  pkt.WritePackedGuid(moverGuid);
  pkt.Append<int8_t>(vehicleExitVoluntary);
  AppendPositionXyz(pkt, posX, posY, posZ);
  pkt.Append<int32_t>(splineId);
  pkt.Append<int8_t>(static_cast<int8_t>(Stop));
  return pkt;
}

} // namespace Firelands::monster_move_wire
