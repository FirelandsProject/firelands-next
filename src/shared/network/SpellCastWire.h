#pragma once

#include <shared/Common.h>
#include <shared/network/WorldPacket.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Firelands {
namespace SpellCastWire {

/// Cataclysm 4.3.4 `SpellCastTargetFlags` (subset).
enum SpellCastTargetFlags : uint32 {
  TARGET_FLAG_NONE = 0x00000000,
  TARGET_FLAG_UNIT = 0x00000002,
};

/// Mask used when reading/writing the primary packed GUID in `SpellTargetData`.
constexpr uint32 ClientTargetPrimaryGuidMask =
    TARGET_FLAG_UNIT | 0x00010000 /*UNIT_MINIPET*/ | 0x00000800 /*GAMEOBJECT*/ |
    0x00000200 /*CORPSE_ENEMY*/ | 0x00008000 /*CORPSE_ALLY*/;

/// Client `SendCastFlags` on CMSG_CAST_SPELL / spell cast request (SpellPacketsCommon).
enum ClientSendCastFlags : uint8 {
  CLIENT_CAST_FLAG_HAS_TRAJECTORY = 0x02,
  CLIENT_CAST_FLAG_HAS_WEIGHT = 0x08,
};

/// Server `CastFlags` on SMSG_SPELL_* (Spell.h).
enum ServerSpellCastFlags : uint32 {
  CAST_FLAG_PENDING = 0x00000001,
  CAST_FLAG_HAS_TRAJECTORY = 0x00000002,
  CAST_FLAG_UNKNOWN_9 = 0x00000100,
  /// `SMSG_SPELL_GO` missile arc (elevation + travel delay); 3.3.5+ `CAST_FLAG_ADJUST_MISSILE`.
  CAST_FLAG_ADJUST_MISSILE = 0x00020000,
  CAST_FLAG_NO_GCD = 0x00040000,
};

/// Optional tail on `SMSG_SPELL_GO` when `CAST_FLAG_ADJUST_MISSILE` is set (after target block).
struct SpellMissileTrajectoryWire {
  float pitch = 0.f;
  uint32 travelTimeMs = 0;
};

enum SpellFailedReason : uint8 {
  SPELL_FAILED_NOT_READY = 69,
  /// Not in spellbook / disabled for this emulated realm (4.3.4 SharedDefines).
  SPELL_FAILED_SPELL_UNAVAILABLE = 109,
  /// `SpellCastResult` for 4.3.4.15595 — confirm vs client/DBC if UI text mismatches.
  SPELL_FAILED_OUT_OF_RANGE = 96,
  /// Matches cmangos-wotlk `SpellDefines.h` / typical 4.x clients; verify vs 15595 if UI mismatches.
  SPELL_FAILED_TOO_CLOSE = 128,
  /// Cataclysm `SharedDefines.h`; verify against build 15595 if mismatch.
  SPELL_FAILED_LINE_OF_SIGHT = 49,
  SPELL_FAILED_NO_POWER = 87,
  SPELL_FAILED_INTERRUPTED = 47,
  /// Client `SPELL_FAILED_SPELL_IS_PASSIVE` (4.3.4); passive spells are not castable.
  SPELL_FAILED_SPELL_IS_PASSIVE = 58,
  /// Caster level below `SpellLevels.dbc` requirement (Trinity `SPELL_FAILED_LOW_CASTLEVEL`).
  SPELL_FAILED_LOW_CASTLEVEL = 73,
};

/// Parses `CMSG_CANCEL_CAST` (spell id + cast id).
bool TryReadClientCancelCast(WorldPacket &packet, uint32 &outSpellId, uint8 &outCastId);

struct ClientCastSpellData {
  uint8 castId = 0;
  int32 spellId = 0;
  int32 misc = 0;
  uint8 sendCastFlags = 0;
  uint32 targetFlags = 0;
  /// Set when `targetFlags` implies a packed unit/object GUID in the packet.
  uint64 unitTargetGuid = 0;
  /// Present when `sendCastFlags` includes `CLIENT_CAST_FLAG_HAS_TRAJECTORY`.
  bool hasMissileTrajectory = false;
  float missilePitch = 0.f;
  float missileSpeed = 0.f;
};

/// Parses CMSG_CAST_SPELL payload after opcode (operator>> SpellCastRequest + target).
/// Returns false if the packet is truncated or uses unsupported optional sections.
bool TryReadClientCastSpell(WorldPacket &packet, ClientCastSpellData &out);

/// Timestamp for `SMSG_SPELL_GO` (client movement clock). Falls back to monotonic ms since
/// first use when `clientMovementTimeMs` is 0.
uint32 ResolveSpellGoTimestampMs(uint32 clientMovementTimeMs);

/// Base `SMSG_SPELL_GO` flags plus `CAST_FLAG_ADJUST_MISSILE` when `useMissile` is true.
uint32 BuildSpellGoCastFlags(bool useMissile);

struct SpellGoMissileResolution {
  bool sendOnWire = false;
  SpellMissileTrajectoryWire trajectory{};
};

/// Resolves missile pitch/travel for `SMSG_SPELL_GO` from client trajectory and/or world positions.
SpellGoMissileResolution ResolveSpellGoMissile(
    ClientCastSpellData const &client, bool hasCasterWorldPosition, float casterX,
    float casterY, float casterZ, bool hasTargetWorldPosition, float targetX,
    float targetY, float targetZ, uint32 fallbackTravelTimeMs);

/// Builds SMSG_SPELL_START (no HitInfo; matches Spell::SendSpellStart for common case).
void BuildSpellStart(WorldPacket &out, uint64 casterGuid, uint8 castId, uint32 spellId,
                     uint32 castFlags, uint32 castFlagsEx, uint32 castTimeMs,
                     uint32 targetFlags, uint64 targetUnitGuid);

/// Builds SMSG_SPELL_GO with HitInfo (self-hit) and target block.
/// `hitTargets` / `hitCount`: no heap required when `hitCount` is small (stack array).
/// When `missile` is non-null, appends elevation + travel time after targets.
void BuildSpellGo(WorldPacket &out, uint64 casterGuid, uint8 castId, uint32 spellId,
                  uint32 castFlags, uint32 castFlagsEx, uint32 castTimeMs,
                  uint64 const *hitTargets, size_t hitCount, uint32 targetFlags,
                  uint64 targetUnitGuid, SpellMissileTrajectoryWire const *missile = nullptr);

void BuildSpellGo(WorldPacket &out, uint64 casterGuid, uint8 castId, uint32 spellId,
                  uint32 castFlags, uint32 castFlagsEx, uint32 castTimeMs,
                  std::vector<uint64> const &hitTargets, uint32 targetFlags,
                  uint64 targetUnitGuid, SpellMissileTrajectoryWire const *missile = nullptr);

void BuildSpellFailure(WorldPacket &out, uint64 casterUnitGuid, uint8 castId,
                       int32 spellId, uint8 reason);

} // namespace SpellCastWire
} // namespace Firelands
