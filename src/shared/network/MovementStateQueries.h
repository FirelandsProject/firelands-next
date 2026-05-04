#pragma once

#include <shared/network/MovementFlags.h>
#include <shared/network/MovementInfo.h>

namespace Firelands {

inline bool MovementHasFlag(MovementInfo const &m, uint32 flag) {
  return (m.flags & flag) != 0;
}

inline bool MovementHasFlag2(MovementInfo const &m, uint16 flag) {
  return (m.flags2 & flag) != 0;
}

/// Client requests walk speed when this bit is set (toggle run/walk).
inline bool MovementPrefersWalkSpeed(MovementInfo const &m) {
  return MovementHasFlag(m, MOVEMENTFLAG_WALKING);
}

inline bool MovementIsRooted(MovementInfo const &m) {
  return MovementHasFlag(m, MOVEMENTFLAG_ROOT);
}

inline bool MovementIsSwimming(MovementInfo const &m) {
  return MovementHasFlag(m, MOVEMENTFLAG_SWIMMING);
}

/// True when the client reports active flight motion (not only “can fly”).
inline bool MovementIsFlyingMotion(MovementInfo const &m) {
  return (m.flags & MOVEMENTFLAG_MASK_MOVING_FLY) != 0;
}

inline bool MovementCanFly(MovementInfo const &m) {
  return MovementHasFlag(m, MOVEMENTFLAG_CAN_FLY);
}

/// In-air / no-gravity states used for gameplay checks (flight paths, hover, GM fly).
inline bool MovementIsAirborneTier(MovementInfo const &m) {
  return MovementIsFlyingMotion(m) || MovementHasFlag(m, MOVEMENTFLAG_DISABLE_GRAVITY) ||
         MovementHasFlag(m, MOVEMENTFLAG_HOVER) ||
         MovementHasFlag(m, MOVEMENTFLAG_SPLINE_ELEVATION);
}

inline bool MovementIsFalling(MovementInfo const &m) {
  return MovementHasFlag(m, MOVEMENTFLAG_FALLING) ||
         MovementHasFlag(m, MOVEMENTFLAG_FALLING_FAR);
}

inline bool MovementIsMovingHorizontally(MovementInfo const &m) {
  return (m.flags & (MOVEMENTFLAG_FORWARD | MOVEMENTFLAG_BACKWARD | MOVEMENTFLAG_STRAFE_LEFT |
                     MOVEMENTFLAG_STRAFE_RIGHT)) != 0;
}

inline bool MovementIsMoving(MovementInfo const &m) {
  return (m.flags & MOVEMENTFLAG_MASK_MOVING) != 0;
}

inline bool MovementIsTurning(MovementInfo const &m) {
  return (m.flags & MOVEMENTFLAG_MASK_TURNING) != 0;
}

inline bool MovementIsWaterWalking(MovementInfo const &m) {
  return MovementHasFlag(m, MOVEMENTFLAG_WATERWALKING);
}

inline bool MovementIsOnTransport(MovementInfo const &m) {
  (void)m;
  // Transport membership is carried outside `MovementInfo` today (wire has a
  // separate transport block). Return false until `MovementInfo` stores it.
  return false;
}

/// `PLAYER_FLAGS` bit mirrored from client/update fields — fatigue / “dead zone”
/// indicator (not the same as breath while swimming).
inline constexpr uint32 kPlayerFlagsIsOutOfBounds = 0x00004000;

inline bool PlayerFlagsIndicatesFatigueBoundary(uint32 playerFlags) {
  return (playerFlags & kPlayerFlagsIsOutOfBounds) != 0;
}

} // namespace Firelands
