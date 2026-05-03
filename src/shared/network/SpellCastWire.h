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
  CAST_FLAG_NO_GCD = 0x00040000,
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
};

struct ClientCastSpellData {
  uint8 castId = 0;
  int32 spellId = 0;
  int32 misc = 0;
  uint8 sendCastFlags = 0;
  uint32 targetFlags = 0;
  /// Set when `targetFlags` implies a packed unit/object GUID in the packet.
  uint64 unitTargetGuid = 0;
};

/// Parses CMSG_CAST_SPELL payload after opcode (operator>> SpellCastRequest + target).
/// Returns false if the packet is truncated or uses unsupported optional sections.
bool TryReadClientCastSpell(WorldPacket &packet, ClientCastSpellData &out);

/// Builds SMSG_SPELL_START (no HitInfo; matches Spell::SendSpellStart for common case).
void BuildSpellStart(WorldPacket &out, uint64 casterGuid, uint8 castId, uint32 spellId,
                     uint32 castFlags, uint32 castFlagsEx, uint32 castTimeMs,
                     uint32 targetFlags, uint64 targetUnitGuid);

/// Builds SMSG_SPELL_GO with HitInfo (self-hit) and target block.
/// `hitTargets` / `hitCount`: no heap required when `hitCount` is small (stack array).
void BuildSpellGo(WorldPacket &out, uint64 casterGuid, uint8 castId, uint32 spellId,
                  uint32 castFlags, uint32 castFlagsEx, uint32 castTimeMs,
                  uint64 const *hitTargets, size_t hitCount, uint32 targetFlags,
                  uint64 targetUnitGuid);

void BuildSpellGo(WorldPacket &out, uint64 casterGuid, uint8 castId, uint32 spellId,
                  uint32 castFlags, uint32 castFlagsEx, uint32 castTimeMs,
                  std::vector<uint64> const &hitTargets, uint32 targetFlags,
                  uint64 targetUnitGuid);

void BuildSpellFailure(WorldPacket &out, uint64 casterUnitGuid, uint8 castId,
                       int32 spellId, uint8 reason);

} // namespace SpellCastWire
} // namespace Firelands
