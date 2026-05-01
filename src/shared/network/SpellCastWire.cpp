#include <shared/network/SpellCastWire.h>

#include <algorithm>

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

static void AppendSpellHitInfo(WorldPacket &data,
                                std::vector<uint64> const &hitTargets) {
  size_t const n = std::min(hitTargets.size(), size_t(255));
  data.Append<uint8>(static_cast<uint8>(n));
  for (size_t i = 0; i < n; ++i)
    data.AppendPackGUID(hitTargets[i]);

  data.Append<uint8>(0); // miss count
}

static void AppendSpellCastDataCore(WorldPacket &data, uint64 casterGuid,
                                    uint64 casterUnitGuid, uint8 castId,
                                    uint32 spellId, uint32 castFlags,
                                    uint32 castFlagsEx, uint32 castTimeMs,
                                    bool withHitInfo,
                                    std::vector<uint64> const *hitTargets,
                                    uint32 targetFlags,
                                    uint64 targetUnitGuid) {
  data.AppendPackGUID(casterGuid);
  data.AppendPackGUID(casterUnitGuid);
  data.Append<uint8>(castId);
  data.Append<uint32>(spellId);
  data.Append<uint32>(castFlags);
  data.Append<uint32>(castFlagsEx);
  data.Append<uint32>(castTimeMs);

  if (withHitInfo && hitTargets) {
    AppendSpellHitInfo(data, *hitTargets);
  }

  AppendSpellTargetData(data, targetFlags, targetUnitGuid);
}

} // namespace

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
    (void)packet.Read<float>();
    (void)packet.Read<float>();
    uint8 hasMovement = packet.Read<uint8>();
    if (hasMovement != 0)
      return false;
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
                           castFlagsEx, castTimeMs, false, nullptr, targetFlags,
                           targetUnitGuid);
}

void BuildSpellGo(WorldPacket &out, uint64 casterGuid, uint8 castId,
                  uint32 spellId, uint32 castFlags, uint32 castFlagsEx,
                  uint32 castTimeMs, std::vector<uint64> const &hitTargets,
                  uint32 targetFlags, uint64 targetUnitGuid) {
  out = WorldPacket(SMSG_SPELL_GO, 200);
  AppendSpellCastDataCore(out, casterGuid, casterGuid, castId, spellId, castFlags,
                           castFlagsEx, castTimeMs, true, &hitTargets, targetFlags,
                           targetUnitGuid);
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
