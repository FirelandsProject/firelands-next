#pragma once

#include <cstdint>

namespace Firelands {

/// Cataclysm `HighGuid::Item` (Trinity TCPP `ObjectGuid.h`): 0x400, shifted by 52 for low GUID.
inline constexpr uint64_t kHighGuidItem = 0x400ULL;

/// Client-visible item ObjectGuid from `item_instance.guid` (low part only in DB).
inline uint64_t MakeItemObjectGuid(uint32_t itemLowGuid) noexcept {
  if (itemLowGuid == 0)
    return 0;
  return static_cast<uint64_t>(itemLowGuid) | (kHighGuidItem << 52);
}

inline void WriteGuidToTwoUint32(uint64_t guid, uint32_t &outLow,
                                 uint32_t &outHigh) noexcept {
  outLow = static_cast<uint32_t>(guid & 0xFFFFFFFFu);
  outHigh = static_cast<uint32_t>(guid >> 32);
}

/// `TypeMask::TYPEMASK_ITEM` — used in `OBJECT_FIELD_TYPE` for `TYPEID_ITEM` creates.
inline constexpr uint32_t kTypeMaskItem = 0x02u;

} // namespace Firelands
