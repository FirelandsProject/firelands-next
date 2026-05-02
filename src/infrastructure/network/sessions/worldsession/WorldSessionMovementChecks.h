#ifndef FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_WORLDSESSION_MOVEMENT_CHECKS_H
#define FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_WORLDSESSION_MOVEMENT_CHECKS_H

#include <shared/network/MovementInfo.h>
#include <shared/network/WorldOpcodes.h>

#include <cmath>

namespace Firelands {

/// Match reference `Firelands::IsValidMapCoord` bounds + sane orientation for
/// movement wire parsing (shared by login position validation and movement echo).
inline constexpr float kWsMapCoordLimit =
    (533.3333f * 64.0f) * 0.5f - 0.5f;

inline constexpr float kWsMaxOrientation = 7.0f; // slightly above 2*pi (~6.283)

inline bool WsIsSaneWorldPosition(MovementInfo const &m) {
  if (!std::isfinite(m.x) || !std::isfinite(m.y) || !std::isfinite(m.z) ||
      !std::isfinite(m.orientation))
    return false;
  if (std::fabs(m.orientation) > kWsMaxOrientation)
    return false;
  return std::fabs(m.x) <= kWsMapCoordLimit && std::fabs(m.y) <= kWsMapCoordLimit &&
         std::fabs(m.z) <= kWsMapCoordLimit;
}

/// Only heartbeat uses trusted packed layout; other MSG_MOVE_* can mis-read Z.
inline bool WsIsTrustedPositionOpcode(WorldOpcode opcode) {
  return opcode == MSG_MOVE_HEARTBEAT;
}

inline bool WsIsClientMovementOpcode(WorldOpcode opcode) {
  switch (opcode) {
  case MSG_MOVE_HEARTBEAT:
  case MSG_MOVE_START_FORWARD:
  case MSG_MOVE_START_BACKWARD:
  case MSG_MOVE_START_STRAFE_LEFT:
  case MSG_MOVE_START_STRAFE_RIGHT:
  case MSG_MOVE_STOP:
  case MSG_MOVE_STOP_STRAFE:
  case MSG_MOVE_START_ASCEND:
  case MSG_MOVE_START_DESCEND:
  case MSG_MOVE_STOP_ASCEND:
  case MSG_MOVE_START_TURN_LEFT:
  case MSG_MOVE_START_TURN_RIGHT:
  case MSG_MOVE_STOP_TURN:
  case MSG_MOVE_START_PITCH_UP:
  case MSG_MOVE_START_PITCH_DOWN:
  case MSG_MOVE_STOP_PITCH:
  case MSG_MOVE_SET_RUN_MODE:
  case MSG_MOVE_SET_WALK_MODE:
  case MSG_MOVE_START_SWIM:
  case MSG_MOVE_STOP_SWIM:
  case MSG_MOVE_JUMP:
  case MSG_MOVE_SET_FACING:
  case MSG_MOVE_FALL_LAND:
    return true;
  default:
    return false;
  }
}

} // namespace Firelands

#endif
