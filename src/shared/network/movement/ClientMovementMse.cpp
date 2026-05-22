#include <shared/network/movement/ClientMovementMse.h>
#include <shared/network/movement/MovementStatusElements.h>
#include <shared/network/BitReader.h>
#include <shared/network/MovementFlags.h>
#include <shared/network/WorldOpcodes.h>

#include <cmath>
#include <cstdint>

namespace Firelands {

#include <shared/network/movement/MovementStatusSequences.inc>

namespace {

MovementStatusElements const *GetClientMovementStatusSequence(uint32 opcode) {
  switch (opcode) {
  case MSG_MOVE_FALL_LAND:
    return MovementFallLand;
  case MSG_MOVE_HEARTBEAT:
    return MovementHeartBeat;
  case MSG_MOVE_JUMP:
    return MovementJump;
  case MSG_MOVE_SET_FACING:
    return MovementSetFacing;
  case MSG_MOVE_START_ASCEND:
    return MovementStartAscend;
  case MSG_MOVE_START_BACKWARD:
    return MovementStartBackward;
  case MSG_MOVE_START_DESCEND:
    return MovementStartDescend;
  case MSG_MOVE_START_FORWARD:
    return MovementStartForward;
  case MSG_MOVE_START_PITCH_DOWN:
    return MovementStartPitchDown;
  case MSG_MOVE_START_PITCH_UP:
    return MovementStartPitchUp;
  case MSG_MOVE_START_STRAFE_LEFT:
    return MovementStartStrafeLeft;
  case MSG_MOVE_START_STRAFE_RIGHT:
    return MovementStartStrafeRight;
  case MSG_MOVE_START_SWIM:
    return MovementStartSwim;
  case MSG_MOVE_START_TURN_LEFT:
    return MovementStartTurnLeft;
  case MSG_MOVE_START_TURN_RIGHT:
    return MovementStartTurnRight;
  case MSG_MOVE_STOP:
    return MovementStop;
  case MSG_MOVE_STOP_ASCEND:
    return MovementStopAscend;
  case MSG_MOVE_STOP_PITCH:
    return MovementStopPitch;
  case MSG_MOVE_STOP_STRAFE:
    return MovementStopStrafe;
  case MSG_MOVE_STOP_SWIM:
    return MovementStopSwim;
  case MSG_MOVE_STOP_TURN:
    return MovementStopTurn;
  case MSG_MOVE_SET_RUN_MODE:
    return MovementSetRunMode;
  case MSG_MOVE_SET_WALK_MODE:
    return MovementSetWalkMode;
  case CMSG_MOVE_FORCE_RUN_SPEED_CHANGE_ACK:
    return MovementForceRunSpeedChangeAck;
  case CMSG_MOVE_FORCE_FLIGHT_SPEED_CHANGE_ACK:
    return MovementForceFlightSpeedChangeAck;
  default:
    return nullptr;
  }
}

template <typename T> bool ReadChecked(ByteBuffer &bb, T &out) {
  if (bb.GetReadPos() + sizeof(T) > bb.Size())
    return false;
  out = bb.Read<T>();
  return true;
}

inline bool ConsumeByteSeq(ByteBuffer &bb, bool present) {
  if (!present)
    return true;
  uint8_t b = 0;
  return ReadChecked(bb, b);
}

} // namespace

bool TryReadClientMovementMse(WorldPacket &packet, uint32 opcode, MovementInfo &move) {
  MovementStatusElements const *sequence = GetClientMovementStatusSequence(opcode);
  if (!sequence)
    return false;

  move = MovementInfo{};

  bool hasMovementFlags = false;
  bool hasMovementFlags2 = false;
  bool hasTimestamp = false;
  bool hasOrientation = false;
  bool hasTransportData = false;
  bool hasTransportTime2 = false;
  bool hasTransportVehicleId = false;
  bool hasPitch = false;
  bool hasFallData = false;
  bool hasFallDirection = false;
  bool hasSplineElevation = false;

  bool guidSend[8] = {};
  bool tguidSend[8] = {};

  BitReader br(packet);

  for (MovementStatusElements const *it = sequence; *it != MSEEnd; ++it) {
    MovementStatusElements const el = *it;
    switch (el) {
    case MSEHasGuidByte0:
    case MSEHasGuidByte1:
    case MSEHasGuidByte2:
    case MSEHasGuidByte3:
    case MSEHasGuidByte4:
    case MSEHasGuidByte5:
    case MSEHasGuidByte6:
    case MSEHasGuidByte7:
      guidSend[static_cast<size_t>(el - MSEHasGuidByte0)] = br.ReadBit();
      break;
    case MSEHasTransportGuidByte0:
    case MSEHasTransportGuidByte1:
    case MSEHasTransportGuidByte2:
    case MSEHasTransportGuidByte3:
    case MSEHasTransportGuidByte4:
    case MSEHasTransportGuidByte5:
    case MSEHasTransportGuidByte6:
    case MSEHasTransportGuidByte7:
      if (hasTransportData)
        tguidSend[static_cast<size_t>(el - MSEHasTransportGuidByte0)] =
            br.ReadBit();
      break;
    case MSEGuidByte0:
    case MSEGuidByte1:
    case MSEGuidByte2:
    case MSEGuidByte3:
    case MSEGuidByte4:
    case MSEGuidByte5:
    case MSEGuidByte6:
    case MSEGuidByte7:
      br.AlignToByteBoundary();
      if (!ConsumeByteSeq(packet, guidSend[static_cast<size_t>(el - MSEGuidByte0)]))
        return false;
      br.ResyncAfterExternalByteReads();
      break;
    case MSETransportGuidByte0:
    case MSETransportGuidByte1:
    case MSETransportGuidByte2:
    case MSETransportGuidByte3:
    case MSETransportGuidByte4:
    case MSETransportGuidByte5:
    case MSETransportGuidByte6:
    case MSETransportGuidByte7:
      if (hasTransportData) {
        br.AlignToByteBoundary();
        if (!ConsumeByteSeq(
                packet, tguidSend[static_cast<size_t>(el - MSETransportGuidByte0)]))
          return false;
        br.ResyncAfterExternalByteReads();
      }
      break;
    case MSEHasMovementFlags:
      hasMovementFlags = !br.ReadBit();
      break;
    case MSEHasMovementFlags2:
      hasMovementFlags2 = !br.ReadBit();
      break;
    case MSEHasTimestamp:
      hasTimestamp = !br.ReadBit();
      break;
    case MSEHasOrientation:
      hasOrientation = !br.ReadBit();
      break;
    case MSEHasTransportData:
      hasTransportData = br.ReadBit();
      break;
    case MSEHasTransportTime2:
      if (hasTransportData)
        hasTransportTime2 = br.ReadBit();
      break;
    case MSEHasVehicleId:
      if (hasTransportData)
        hasTransportVehicleId = br.ReadBit();
      break;
    case MSEHasPitch:
      hasPitch = !br.ReadBit();
      break;
    case MSEHasFallData:
      hasFallData = br.ReadBit();
      break;
    case MSEHasFallDirection:
      if (hasFallData)
        hasFallDirection = br.ReadBit();
      break;
    case MSEHasSplineElevation:
      hasSplineElevation = !br.ReadBit();
      break;
    case MSEHasSpline:
      (void)br.ReadBit();
      break;
    case MSEHasHeightChangeFailed:
      (void)br.ReadBit();
      break;
    case MSEZeroBit:
    case MSEOneBit:
      (void)br.ReadBit();
      break;
    case MSEFlushBits:
      br.AlignToByteBoundary();
      break;
    case MSEMovementFlags:
      if (hasMovementFlags)
        move.flags = br.ReadBits(30) & kMovementFlagsWireMask;
      break;
    case MSEMovementFlags2:
      if (hasMovementFlags2)
        move.flags2 =
            static_cast<uint16>(br.ReadBits(12) & kMovementFlags2WireMask);
      break;
    case MSETimestamp:
      if (hasTimestamp) {
        br.AlignToByteBoundary();
        if (!ReadChecked(packet, move.time))
          return false;
        br.ResyncAfterExternalByteReads();
      }
      break;
    case MSEPositionX:
      br.AlignToByteBoundary();
      if (!ReadChecked(packet, move.x))
        return false;
      br.ResyncAfterExternalByteReads();
      break;
    case MSEPositionY:
      br.AlignToByteBoundary();
      if (!ReadChecked(packet, move.y))
        return false;
      br.ResyncAfterExternalByteReads();
      break;
    case MSEPositionZ:
      br.AlignToByteBoundary();
      if (!ReadChecked(packet, move.z))
        return false;
      br.ResyncAfterExternalByteReads();
      break;
    case MSEOrientation:
      if (hasOrientation) {
        br.AlignToByteBoundary();
        if (!ReadChecked(packet, move.orientation))
          return false;
        br.ResyncAfterExternalByteReads();
      }
      break;
    case MSETransportPositionX:
      if (hasTransportData) {
        br.AlignToByteBoundary();
        float v = 0.0f;
        if (!ReadChecked(packet, v))
          return false;
        (void)v;
        br.ResyncAfterExternalByteReads();
      }
      break;
    case MSETransportPositionY:
      if (hasTransportData) {
        br.AlignToByteBoundary();
        float v = 0.0f;
        if (!ReadChecked(packet, v))
          return false;
        (void)v;
        br.ResyncAfterExternalByteReads();
      }
      break;
    case MSETransportPositionZ:
      if (hasTransportData) {
        br.AlignToByteBoundary();
        float v = 0.0f;
        if (!ReadChecked(packet, v))
          return false;
        (void)v;
        br.ResyncAfterExternalByteReads();
      }
      break;
    case MSETransportOrientation:
      if (hasTransportData) {
        br.AlignToByteBoundary();
        float v = 0.0f;
        if (!ReadChecked(packet, v))
          return false;
        (void)v;
        br.ResyncAfterExternalByteReads();
      }
      break;
    case MSETransportSeat:
      if (hasTransportData) {
        br.AlignToByteBoundary();
        int8_t seat = 0;
        if (!ReadChecked(packet, seat))
          return false;
        (void)seat;
        br.ResyncAfterExternalByteReads();
      }
      break;
    case MSETransportTime:
      if (hasTransportData) {
        br.AlignToByteBoundary();
        uint32_t t = 0;
        if (!ReadChecked(packet, t))
          return false;
        (void)t;
        br.ResyncAfterExternalByteReads();
      }
      break;
    case MSETransportTime2:
      if (hasTransportData && hasTransportTime2) {
        br.AlignToByteBoundary();
        uint32_t t = 0;
        if (!ReadChecked(packet, t))
          return false;
        (void)t;
        br.ResyncAfterExternalByteReads();
      }
      break;
    case MSETransportVehicleId:
      if (hasTransportData && hasTransportVehicleId) {
        br.AlignToByteBoundary();
        uint32_t vid = 0;
        if (!ReadChecked(packet, vid))
          return false;
        (void)vid;
        br.ResyncAfterExternalByteReads();
      }
      break;
    case MSEPitch:
      if (hasPitch) {
        br.AlignToByteBoundary();
        float p = 0.0f;
        if (!ReadChecked(packet, p))
          return false;
        (void)p;
        br.ResyncAfterExternalByteReads();
      }
      break;
    case MSEFallTime:
      if (hasFallData) {
        br.AlignToByteBoundary();
        if (!ReadChecked(packet, move.fallTime))
          return false;
        br.ResyncAfterExternalByteReads();
      }
      break;
    case MSEFallVerticalSpeed:
      if (hasFallData) {
        br.AlignToByteBoundary();
        float v = 0.0f;
        if (!ReadChecked(packet, v))
          return false;
        (void)v;
        br.ResyncAfterExternalByteReads();
      }
      break;
    case MSEFallCosAngle:
      if (hasFallData && hasFallDirection) {
        br.AlignToByteBoundary();
        float v = 0.0f;
        if (!ReadChecked(packet, v))
          return false;
        (void)v;
        br.ResyncAfterExternalByteReads();
      }
      break;
    case MSEFallSinAngle:
      if (hasFallData && hasFallDirection) {
        br.AlignToByteBoundary();
        float v = 0.0f;
        if (!ReadChecked(packet, v))
          return false;
        (void)v;
        br.ResyncAfterExternalByteReads();
      }
      break;
    case MSEFallHorizontalSpeed:
      if (hasFallData && hasFallDirection) {
        br.AlignToByteBoundary();
        float v = 0.0f;
        if (!ReadChecked(packet, v))
          return false;
        (void)v;
        br.ResyncAfterExternalByteReads();
      }
      break;
    case MSESplineElevation:
      if (hasSplineElevation) {
        br.AlignToByteBoundary();
        float v = 0.0f;
        if (!ReadChecked(packet, v))
          return false;
        (void)v;
        br.ResyncAfterExternalByteReads();
      }
      break;
    case MSECounter: {
      br.AlignToByteBoundary();
      uint32_t c = 0;
      if (!ReadChecked(packet, c))
        return false;
      (void)c;
      br.ResyncAfterExternalByteReads();
      break;
    }
    case MSEExtraElement:
    case MSEExtraFloat: {
      br.AlignToByteBoundary();
      float extra = 0.f;
      if (!ReadChecked(packet, extra))
        return false;
      (void)extra;
      br.ResyncAfterExternalByteReads();
      break;
    }
    case MSEExtraInt8:
    case MSEExtraTwoBits:
      return false;
    default:
      return false;
    }
  }

  if (!std::isfinite(move.x) || !std::isfinite(move.y) || !std::isfinite(move.z) ||
      !std::isfinite(move.orientation))
    return false;

  return true;
}

} // namespace Firelands
