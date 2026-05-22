#include <shared/network/SpellCastWire.h>

#include <shared/network/movement/ClientMovementMse.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace Firelands {
namespace SpellCastWire {
namespace {

static void AppendSpellTargetData(WorldPacket &data, uint32 targetFlags,
                                  uint64 unitGuidForUnitMask) {
  data.Append<uint32>(targetFlags);
  if (targetFlags & ClientTargetPrimaryGuidMask) {
    data.AppendPackGUID(unitGuidForUnitMask);
  }
  if (targetFlags & 0x00000010) { // TARGET_FLAG_ITEM
    data.AppendPackGUID(0);
  }
  if (targetFlags & 0x00000020) { // TARGET_FLAG_SOURCE_LOCATION
    data.AppendPackGUID(0);
    data.Append<float>(0.0f);
    data.Append<float>(0.0f);
    data.Append<float>(0.0f);
  }
  if (targetFlags & 0x00000040) { // TARGET_FLAG_DEST_LOCATION
    data.AppendPackGUID(0);
    data.Append<float>(0.0f);
    data.Append<float>(0.0f);
    data.Append<float>(0.0f);
  }
  if (targetFlags & 0x00002000) { // TARGET_FLAG_STRING
    data.Append<uint8>(0);
  }
}

static void AppendSpellHitInfo(WorldPacket &data, uint64 const *hitTargets,
                               size_t hitCount) {
  // Cataclysm 4.3.4 `SpellHitInfo`: each hit is `ObjectGuid` as raw uint64 (not packed).
  size_t const n = std::min(hitCount, size_t(255));
  data.Append<uint8>(static_cast<uint8>(n));
  for (size_t i = 0; i < n; ++i)
    data.Append<uint64>(hitTargets[i]);

  data.Append<uint8>(0); // miss count
}

static void AppendSpellCastDataCore(WorldPacket &data, uint64 casterGuid,
                                    uint64 casterUnitGuid, uint8 castId,
                                    uint32 spellId, uint32 castFlags,
                                    uint32 castFlagsEx, uint32 castTimeMs,
                                    bool withHitInfo, uint64 const *hitTargets,
                                    size_t hitCount, uint32 targetFlags,
                                    uint64 targetUnitGuid,
                                    SpellMissileTrajectoryWire const *missile) {
  data.AppendPackGUID(casterGuid);
  data.AppendPackGUID(casterUnitGuid);
  data.Append<uint8>(castId);
  data.Append<uint32>(spellId);
  data.Append<uint32>(castFlags);
  data.Append<uint32>(castFlagsEx);
  data.Append<uint32>(castTimeMs);

  if (withHitInfo && hitTargets != nullptr && hitCount != 0u)
    AppendSpellHitInfo(data, hitTargets, hitCount);

  AppendSpellTargetData(data, targetFlags, targetUnitGuid);

  if ((castFlags & CAST_FLAG_ADJUST_MISSILE) != 0u && missile != nullptr) {
    data.Append<float>(missile->pitch);
    data.Append<int32>(static_cast<int32>(missile->travelTimeMs));
  }
}

static float HorizontalDistance(float x0, float y0, float x1, float y1) {
  float const dx = x1 - x0;
  float const dy = y1 - y0;
  return std::sqrt(dx * dx + dy * dy);
}

static uint32 ComputeTravelMsFromSpeed(float speed, float horizontalDistance,
                                       uint32 fallbackMs) {
  if (speed > 0.01f && horizontalDistance > 0.01f) {
    double const ms = static_cast<double>(horizontalDistance) / static_cast<double>(speed) *
                      1000.0;
    if (ms > 0.0 && ms <= static_cast<double>(UINT32_MAX))
      return static_cast<uint32>(ms);
  }
  return fallbackMs;
}

} // namespace

uint32 BuildSpellGoCastFlags(bool useMissile) {
  uint32 flags = CAST_FLAG_UNKNOWN_9;
  if (useMissile)
    flags |= CAST_FLAG_ADJUST_MISSILE;
  return flags;
}

SpellGoMissileResolution ResolveSpellGoMissile(
    ClientCastSpellData const &client, bool hasCasterWorldPosition, float casterX,
    float casterY, float casterZ, bool hasTargetWorldPosition, float targetX,
    float targetY, float targetZ, uint32 fallbackTravelTimeMs) {
  SpellGoMissileResolution result{};
  // Reference `SpellCastTargets::HasTraj()` is `m_speed != 0` (from client trajectory).
  if (!client.hasMissileTrajectory || client.missileSpeed <= 0.01f)
    return result;

  float horizontal = 0.f;
  if (hasCasterWorldPosition && hasTargetWorldPosition)
    horizontal = HorizontalDistance(casterX, casterY, targetX, targetY);

  result.sendOnWire = true;
  result.trajectory.pitch = client.missilePitch;
  result.trajectory.travelTimeMs =
      ComputeTravelMsFromSpeed(client.missileSpeed, horizontal, fallbackTravelTimeMs);
  return result;
}

uint32 ResolveSpellGoTimestampMs(uint32 clientMovementTimeMs) {
  if (clientMovementTimeMs != 0u)
    return clientMovementTimeMs;
  static auto const kStart = std::chrono::steady_clock::now();
  return static_cast<uint32>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - kStart)
                                 .count());
}

bool TryReadClientCancelCast(WorldPacket &packet, uint32 &outSpellId, uint8 &outCastId) {
  outSpellId = 0;
  outCastId = 0;
  if (packet.Size() - packet.GetReadPos() < sizeof(uint32) + sizeof(uint8))
    return false;
  outSpellId = packet.Read<uint32>();
  outCastId = packet.Read<uint8>();
  return true;
}

bool TryReadClientCastSpell(WorldPacket &packet, ClientCastSpellData &out) {
  out = {};

  if (packet.Size() - packet.GetReadPos() < 1 + 4 + 4 + 1)
    return false;

  out.castId = packet.Read<uint8>();
  out.spellId = packet.Read<int32>();
  out.misc = packet.Read<int32>();
  out.sendCastFlags = packet.Read<uint8>();

  if (packet.Size() - packet.GetReadPos() < 4)
    return false;
  out.targetFlags = packet.Read<uint32>();

  if (out.targetFlags & ClientTargetPrimaryGuidMask) {
    if (packet.Size() - packet.GetReadPos() < 1)
      return false;
    out.unitTargetGuid = packet.ReadPackedGuid();
  }

  if (out.targetFlags & 0x00000010) { // item
    if (packet.Size() - packet.GetReadPos() < 1)
      return false;
    (void)packet.ReadPackedGuid();
  }
  if (out.targetFlags & 0x00000020) { // src
    if (packet.Size() - packet.GetReadPos() < 1 + 4 + 4 + 4)
      return false;
    (void)packet.ReadPackedGuid();
    (void)packet.Read<float>();
    (void)packet.Read<float>();
    (void)packet.Read<float>();
  }
  if (out.targetFlags & 0x00000040) { // dst
    if (packet.Size() - packet.GetReadPos() < 1 + 4 + 4 + 4)
      return false;
    (void)packet.ReadPackedGuid();
    (void)packet.Read<float>();
    (void)packet.Read<float>();
    (void)packet.Read<float>();
  }
  if (out.targetFlags & 0x00002000) { // string
    if (packet.Size() - packet.GetReadPos() < 1)
      return false;
    std::string tmp;
    while (packet.GetReadPos() < packet.Size()) {
      char c = static_cast<char>(packet.Read<uint8>());
      if (c == 0)
        break;
      tmp.push_back(c);
    }
  }

  if (out.sendCastFlags & CLIENT_CAST_FLAG_HAS_TRAJECTORY) {
    if (packet.Size() - packet.GetReadPos() < 4 + 4 + 1)
      return false;
    out.missilePitch = packet.Read<float>();
    out.missileSpeed = packet.Read<float>();
    out.hasMissileTrajectory = true;
    uint8 const movementData = packet.Read<uint8>();
    if (movementData != 0) {
      if (packet.Size() - packet.GetReadPos() < 4 + 1)
        return false;
      uint32 const moveOpcode = packet.Read<uint32>();
      if (packet.Size() - packet.GetReadPos() < 1)
        return false;
      (void)packet.ReadPackedGuid();
      MovementInfo move{};
      (void)TryReadClientMovementMse(packet, moveOpcode, move);
    }
  }

  if (out.sendCastFlags & CLIENT_CAST_FLAG_HAS_WEIGHT) {
    if (packet.Size() - packet.GetReadPos() < 4)
      return false;
    uint32 weightCount = packet.Read<uint32>();
    constexpr uint32 kMaxWeight = 32;
    if (weightCount > kMaxWeight)
      return false;
    for (uint32 i = 0; i < weightCount; ++i) {
      if (packet.Size() - packet.GetReadPos() < 1 + 4 + 4)
        return false;
      (void)packet.Read<uint8>();
      (void)packet.Read<int32>();
      (void)packet.Read<uint32>();
    }
  }

  return true;
}

void BuildSpellStart(WorldPacket &out, uint64 casterGuid, uint8 castId,
                     uint32 spellId, uint32 castFlags, uint32 castFlagsEx,
                     uint32 castTimeMs, uint32 targetFlags,
                     uint64 targetUnitGuid) {
  out = WorldPacket(SMSG_SPELL_START, 200);
  AppendSpellCastDataCore(out, casterGuid, casterGuid, castId, spellId, castFlags,
                           castFlagsEx, castTimeMs, false, nullptr, 0, targetFlags,
                           targetUnitGuid, nullptr);
}

void BuildSpellGo(WorldPacket &out, uint64 casterGuid, uint8 castId,
                  uint32 spellId, uint32 castFlags, uint32 castFlagsEx,
                  uint32 castTimeMs, uint64 const *hitTargets, size_t hitCount,
                  uint32 targetFlags, uint64 targetUnitGuid,
                  SpellMissileTrajectoryWire const *missile) {
  out = WorldPacket(SMSG_SPELL_GO, 200);
  AppendSpellCastDataCore(out, casterGuid, casterGuid, castId, spellId, castFlags,
                           castFlagsEx, castTimeMs, true, hitTargets, hitCount,
                           targetFlags, targetUnitGuid, missile);
}

void BuildSpellGo(WorldPacket &out, uint64 casterGuid, uint8 castId,
                  uint32 spellId, uint32 castFlags, uint32 castFlagsEx,
                  uint32 castTimeMs, std::vector<uint64> const &hitTargets,
                  uint32 targetFlags, uint64 targetUnitGuid,
                  SpellMissileTrajectoryWire const *missile) {
  BuildSpellGo(out, casterGuid, castId, spellId, castFlags, castFlagsEx, castTimeMs,
               hitTargets.data(), hitTargets.size(), targetFlags, targetUnitGuid, missile);
}

void BuildSpellFailure(WorldPacket &out, uint64 casterUnitGuid, uint8 castId,
                       int32 spellId, uint8 reason) {
  out = WorldPacket(SMSG_SPELL_FAILURE, 32);
  out.AppendPackGUID(casterUnitGuid);
  out.Append<uint8>(castId);
  out.Append<int32>(spellId);
  out.Append<uint8>(reason);
}

} // namespace SpellCastWire
} // namespace Firelands
