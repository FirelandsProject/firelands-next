#pragma once

#include <cstdint>

namespace Firelands {

/// Cataclysm `HighGuid::Item`: 0x400, shifted by 52 for low GUID.
inline constexpr uint64_t kHighGuidItem = 0x400ULL;

/// Cataclysm 4.3.4 preservation `HighGuid::Unit` (Blizz-style **F130** creature GUID).
/// Raw `creature.guid` from the DB is only the low counter; it must be combined with the
/// template entry and this high prefix or the 4.3.4 client treats the object as a player
/// GUID and will not render creatures correctly.
inline constexpr uint64_t kHighGuidUnit = 0xF13ULL;

/// Wire-format creature `ObjectGuid` (`ObjectGuid(HighGuid::Unit, entry, counter)`).
inline uint64_t MakeCreatureObjectGuid(uint32_t creatureEntry,
                                       uint32_t spawnCounterLow) noexcept {
  if (spawnCounterLow == 0)
    return 0;
  return static_cast<uint64_t>(spawnCounterLow) |
         (static_cast<uint64_t>(creatureEntry) << 32) |
         (kHighGuidUnit << 52);
}

/// Inverse of `MakeCreatureObjectGuid` for the template entry field (spawn counter ignored).
/// Entry occupies bits 32–51; `kHighGuidUnit` uses 52+ (mask required).
inline uint32_t ExtractCreatureEntryFromUnitObjectGuid(uint64_t objectGuid) noexcept {
  if (objectGuid == 0)
    return 0;
  return static_cast<uint32_t>((objectGuid >> 32) & 0x000FFFFFu);
}

/// Player `ObjectGuid` on the wire (`HighGuid::Player == 0` in 4.3.4): raw uint64 counter.
inline uint64_t MakePlayerObjectGuid(uint32_t playerLowGuid) noexcept {
  return playerLowGuid != 0 ? static_cast<uint64_t>(playerLowGuid) : 0u;
}

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
