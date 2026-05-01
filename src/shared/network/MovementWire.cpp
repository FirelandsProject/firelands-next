#include <shared/network/MovementWire.h>
#include <shared/network/BitReader.h>

#include <cmath>

namespace Firelands {

namespace {

bool FiniteVec(float x, float y, float z, float o) {
  return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(o);
}

// Derived from firelands-cata-ref MovementStructures.cpp MovementHeartBeat[]
// and Player::ReadMovementInfo (Player.cpp ~28170).
//
// Key layout (different from all other movement opcodes):
//   1. Z, X, Y as raw floats at the very start (MSEPositionZ/X/Y)
//   2. Bit section: Has* presence flags
//   3. Raw byte section: GUID bytes, optional data (orientation, fall, pitch…)
//
// Bit-flag inversion: some Has* are written as !flag (so we invert on read).
bool TryReadMovementHeartbeat(WorldPacket &packet, MovementInfo &move) {
  if (packet.GetReadPos() + 12 > packet.Size())
    return false;

  // --- Raw position at the start (before any bit fields) ---
  move.z = packet.Read<float>(); // MSEPositionZ
  move.x = packet.Read<float>(); // MSEPositionX
  move.y = packet.Read<float>(); // MSEPositionY

  if (!std::isfinite(move.x) || !std::isfinite(move.y) || !std::isfinite(move.z))
    return false;

  // --- Bit section ---
  BitReader br(packet);

  bool hasPitch          = !br.ReadBit(); // MSEHasPitch          (written as !flag)
  bool hasTimestamp      = !br.ReadBit(); // MSEHasTimestamp       (written as !flag)
  bool hasFallData       =  br.ReadBit(); // MSEHasFallData
  bool hasMovFlags2      = !br.ReadBit(); // MSEHasMovementFlags2  (written as !flag)
  bool hasTransport      =  br.ReadBit(); // MSEHasTransportData

  uint8 hasGuid[8] = {};
  hasGuid[7] = br.ReadBit(); // MSEHasGuidByte7
  hasGuid[1] = br.ReadBit(); // MSEHasGuidByte1
  hasGuid[0] = br.ReadBit(); // MSEHasGuidByte0
  hasGuid[4] = br.ReadBit(); // MSEHasGuidByte4
  hasGuid[2] = br.ReadBit(); // MSEHasGuidByte2

  bool hasOrientation    = !br.ReadBit(); // MSEHasOrientation     (written as !flag)

  hasGuid[5] = br.ReadBit(); // MSEHasGuidByte5
  hasGuid[3] = br.ReadBit(); // MSEHasGuidByte3

  bool hasSplineElev     = !br.ReadBit(); // MSEHasSplineElevation (written as !flag)
  (void)br.ReadBit();                     // MSEHasSpline          (not needed)
  (void)br.ReadBit();                     // MSEZeroBit            (height_change_failed)

  hasGuid[6] = br.ReadBit(); // MSEHasGuidByte6

  bool hasMovFlags       = !br.ReadBit(); // MSEHasMovementFlags   (written as !flag)

  // Transport GUID presence bits (only when onTransport)
  uint8 hasTGuid[8] = {};
  bool hasVehicleId = false, hasTransportTime2 = false;
  if (hasTransport) {
    hasVehicleId      = br.ReadBit(); // MSEHasVehicleId
    hasTGuid[4]       = br.ReadBit(); // MSEHasTransportGuidByte4
    hasTGuid[2]       = br.ReadBit(); // MSEHasTransportGuidByte2
    hasTransportTime2 = br.ReadBit(); // MSEHasTransportTime2
    hasTGuid[5]       = br.ReadBit(); // MSEHasTransportGuidByte5
    hasTGuid[7]       = br.ReadBit(); // MSEHasTransportGuidByte7
    hasTGuid[6]       = br.ReadBit(); // MSEHasTransportGuidByte6
    hasTGuid[0]       = br.ReadBit(); // MSEHasTransportGuidByte0
    hasTGuid[3]       = br.ReadBit(); // MSEHasTransportGuidByte3
    hasTGuid[1]       = br.ReadBit(); // MSEHasTransportGuidByte1
  }

  bool hasFallDir = false;
  if (hasFallData)
    hasFallDir = br.ReadBit(); // MSEHasFallDirection

  if (hasMovFlags)
    move.flags = br.ReadBits(30);  // MSEMovementFlags
  if (hasMovFlags2)
    move.flags2 = static_cast<uint16>(br.ReadBits(12)); // MSEMovementFlags2

  // Flush remaining bits before switching to raw byte reads
  br.AlignToByteBoundary();

  // --- GUID bytes (order from MovementHeartBeat[]: 3,6,1,7,2,5,0,4) ---
  // ReadByteSeq: only reads a packet byte when the presence bit is non-zero.
  auto SkipGuidByte = [&](uint8 present) {
    if (present)
      packet.Read<uint8>();
  };
  SkipGuidByte(hasGuid[3]);
  SkipGuidByte(hasGuid[6]);
  SkipGuidByte(hasGuid[1]);
  SkipGuidByte(hasGuid[7]);
  SkipGuidByte(hasGuid[2]);
  SkipGuidByte(hasGuid[5]);
  SkipGuidByte(hasGuid[0]);
  SkipGuidByte(hasGuid[4]);

  // --- Transport data ---
  if (hasTransport) {
    auto SkipTGuidByte = [&](uint8 present) {
      if (present)
        packet.Read<uint8>();
    };
    (void)packet.Read<float>();  // MSETransportPositionZ
    (void)packet.Read<int8>();   // MSETransportSeat
    (void)packet.Read<float>();  // MSETransportOrientation
    SkipTGuidByte(hasTGuid[4]); // MSETransportGuidByte4
    (void)packet.Read<float>();  // MSETransportPositionY
    (void)packet.Read<uint32>(); // MSETransportTime
    (void)packet.Read<float>();  // MSETransportPositionX
    SkipTGuidByte(hasTGuid[5]); // MSETransportGuidByte5
    SkipTGuidByte(hasTGuid[1]); // MSETransportGuidByte1
    SkipTGuidByte(hasTGuid[3]); // MSETransportGuidByte3
    SkipTGuidByte(hasTGuid[7]); // MSETransportGuidByte7
    if (hasVehicleId)
      (void)packet.Read<uint32>(); // MSETransportVehicleId
    if (hasTransportTime2)
      (void)packet.Read<uint32>(); // MSETransportTime2
    SkipTGuidByte(hasTGuid[2]); // MSETransportGuidByte2
    SkipTGuidByte(hasTGuid[0]); // MSETransportGuidByte0
    SkipTGuidByte(hasTGuid[6]); // MSETransportGuidByte6
  }

  // --- Orientation (MSEOrientation) ---
  if (hasOrientation)
    move.orientation = packet.Read<float>();
  else
    move.orientation = 0.0f;

  // --- Fall data ---
  if (hasFallData) {
    (void)packet.Read<float>();  // MSEFallVerticalSpeed
    (void)packet.Read<uint32>(); // MSEFallTime
    if (hasFallDir) {
      (void)packet.Read<float>(); // MSEFallHorizontalSpeed
      (void)packet.Read<float>(); // MSEFallCosAngle
      (void)packet.Read<float>(); // MSEFallSinAngle
    }
  }

  if (hasPitch)
    (void)packet.Read<float>(); // MSEPitch

  if (hasSplineElev)
    (void)packet.Read<float>(); // MSESplineElevation

  if (hasTimestamp)
    move.time = packet.Read<uint32>(); // MSETimestamp

  return std::isfinite(move.orientation);
}

// Non-heartbeat MSG_MOVE_*: leading PackedGuid, then bit header + payload
// (WowPacketParser MovementHandler.ReadMovementInfo420 + PackedGuid).
bool TryReadMovementPackedGuidMessage(WorldPacket &packet, MovementInfo &move) {
  if (packet.GetReadPos() >= packet.Size())
    return false;

  (void)packet.ReadPackedGuid();

  BitReader br(packet);

  move.flags = br.ReadBits(30);
  move.flags2 = static_cast<uint16>(br.ReadBits(12));

  bool onTransport = br.ReadBit();
  bool hasInterpolatedMovement = false;
  bool time3 = false;
  if (onTransport) {
    hasInterpolatedMovement = br.ReadBit();
    time3 = br.ReadBit();
  }

  bool swimming = br.ReadBit();
  bool interPolatedTurning = br.ReadBit();
  bool jumping = false;
  if (interPolatedTurning)
    jumping = br.ReadBit();

  bool splineElevation = br.ReadBit();
  (void)br.ReadBit(); // HasSplineData

  br.AlignToByteBoundary();

  (void)packet.ReadPackedGuid(); // GUID 2
  move.time = packet.Read<uint32>();
  move.x = packet.Read<float>();
  move.y = packet.Read<float>();
  move.z = packet.Read<float>();
  move.orientation = packet.Read<float>();

  if (!FiniteVec(move.x, move.y, move.z, move.orientation))
    return false;

  if (onTransport) {
    (void)packet.ReadPackedGuid();
    (void)packet.Read<float>();
    (void)packet.Read<float>();
    (void)packet.Read<float>();
    (void)packet.Read<float>(); // Vector4 transport offset
    (void)packet.Read<uint8>();   // seat
    (void)packet.Read<int32>();  // transport time
    if (hasInterpolatedMovement)
      (void)packet.Read<int32>();
    if (time3)
      (void)packet.Read<int32>();
  }

  if (swimming)
    (void)packet.Read<float>();

  if (interPolatedTurning) {
    (void)packet.Read<int32>();
    (void)packet.Read<float>();
    if (jumping) {
      (void)packet.Read<float>();
      (void)packet.Read<float>();
      (void)packet.Read<float>();
    }
  }

  if (splineElevation)
    (void)packet.Read<float>();

  return true;
}

} // namespace

bool TryReadClientMovement(WorldPacket &packet, WorldOpcode opcode,
                           MovementInfo &move) {
  move = MovementInfo{};

  switch (opcode) {
  case MSG_MOVE_HEARTBEAT:
    return TryReadMovementHeartbeat(packet, move);
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
    return TryReadMovementPackedGuidMessage(packet, move);
  default:
    return false;
  }
}

} // namespace Firelands
