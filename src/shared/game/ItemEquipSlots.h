#pragma once

#include <shared/Common.h>
#include <shared/game/InventorySlots.h>
#include <array>
#include <optional>

namespace Firelands {

/// WoW InventoryType values (ItemTemplate.InventoryType).
enum ItemInventoryType : uint8 {
  INVTYPE_NON_EQUIP = 0,
  INVTYPE_HEAD = 1,
  INVTYPE_NECK = 2,
  INVTYPE_SHOULDERS = 3,
  INVTYPE_BODY = 4,
  INVTYPE_CHEST = 5,
  INVTYPE_WAIST = 6,
  INVTYPE_LEGS = 7,
  INVTYPE_FEET = 8,
  INVTYPE_WRISTS = 9,
  INVTYPE_HANDS = 10,
  INVTYPE_FINGER = 11,
  INVTYPE_TRINKET = 12,
  INVTYPE_WEAPON = 13,
  INVTYPE_SHIELD = 14,
  INVTYPE_RANGED = 15,
  INVTYPE_CLOAK = 16,
  INVTYPE_2HWEAPON = 17,
  INVTYPE_TABARD = 19,
  INVTYPE_ROBE = 20,
  INVTYPE_WEAPONMAINHAND = 21,
  INVTYPE_WEAPONOFFHAND = 22,
  INVTYPE_HOLDABLE = 23,
  INVTYPE_THROWN = 25,
  INVTYPE_RANGEDRIGHT = 26,
};

/// Tracks finger/trinket pairs while assigning starter gear.
class EquipSlotAllocator {
public:
  std::optional<uint8_t> TryEquipSlot(uint8_t inventoryType);

private:
  std::array<bool, kEquipmentSlotCount> occupied{};
  uint8_t nextFinger = 10;   // EQUIPMENT_SLOT_FINGER1
  uint8_t nextTrinket = 12; // EQUIPMENT_SLOT_TRINKET1
};

} // namespace Firelands
