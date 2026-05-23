#pragma once

#include <shared/Common.h>
#include <shared/network/WorldPacket.h>

#include <vector>

namespace Firelands {
namespace AuraUpdateWire {

/// Cataclysm 4.3.4 `AuraFlag` (WowPacketParser / packet reference). On the wire these are `uint16`
/// since build 4.2.0.14333, not a single byte.
enum AuraFlag : uint16 {
  kEmpty = 0x0000,
  kEffectIndex0 = 0x0001,
  kEffectIndex1 = 0x0002,
  kEffectIndex2 = 0x0004,
  /// `AFLAG_CASTER` — set when caster GUID is omitted (typically self-cast).
  kCaster = 0x0008,
  kNotCaster = kCaster,
  kPositive = 0x0010,
  kDuration = 0x0020,
  kNegative = 0x0080,
  /// `AFLAG_ANY_EFFECT_AMOUNT_SENT` — `Spell.dbc` `AttributesEx8` `SPELL_ATTR8_AURA_SEND_AMOUNT`.
  kAnyEffectAmountSent = 0x0040,
};

struct AuraApplyParams {
  uint8 visualSlot = 0;
  uint32 spellId = 0;
  /// `SpellEffect.dbc` effect index (0–2) for the apply-aura row.
  uint8 effectIndex = 0;
  /// Bits 0–2: all apply-aura effect indices on the spell (defaults to `1 << effectIndex`).
  uint8 activeEffectMask = 0;
  uint8 casterLevel = 1;
  uint8 stackCount = 1;
  uint64 casterGuid = 0;
  uint32 durationMs = 0;
  uint32 remainingMs = 0;
  /// When true, `remainingMs` is written as-is (including 0). Otherwise 0 means "use duration".
  bool explicitRemaining = false;
  bool isNegative = false;
  /// When set, appends effect magnitude for `effectIndex` (required for e.g. Rejuvenation).
  bool sendEffectAmount = false;
  int32 effectAmount = 0;
};

/// Builds `SMSG_AURA_UPDATE` (apply or refresh) for `unitGuid`.
void BuildAuraApply(WorldPacket &out, uint64 unitGuid, AuraApplyParams const &params);

/// Builds `SMSG_AURA_UPDATE_ALL` — full visible aura list (login / resync after remove).
void BuildAuraUpdateAll(WorldPacket &out, uint64 unitGuid,
                        std::vector<AuraApplyParams> const &auras);

/// Builds `SMSG_AURA_UPDATE` telling the client to remove the aura in `visualSlot`.
void BuildAuraRemove(WorldPacket &out, uint64 unitGuid, uint8 visualSlot);

/// Same layout as apply but `remainingMs` is 0 (client countdown finished).
void BuildAuraExpire(WorldPacket &out, uint64 unitGuid, AuraApplyParams const &params);

} // namespace AuraUpdateWire
} // namespace Firelands
