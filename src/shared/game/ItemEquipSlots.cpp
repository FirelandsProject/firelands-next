#include <shared/game/ItemEquipSlots.h>

namespace Firelands {

std::optional<uint8_t> EquipSlotAllocator::TryEquipSlot(uint8_t inventoryType) {
  uint8_t slot = 0xFF;

  switch (inventoryType) {
  case INVTYPE_HEAD:
    slot = 0;
    break;
  case INVTYPE_NECK:
    slot = 1;
    break;
  case INVTYPE_SHOULDERS:
    slot = 2;
    break;
  case INVTYPE_BODY:
    slot = 3;
    break;
  case INVTYPE_CHEST:
  case INVTYPE_ROBE:
    slot = 4;
    break;
  case INVTYPE_WAIST:
    slot = 5;
    break;
  case INVTYPE_LEGS:
    slot = 6;
    break;
  case INVTYPE_FEET:
    slot = 7;
    break;
  case INVTYPE_WRISTS:
    slot = 8;
    break;
  case INVTYPE_HANDS:
    slot = 9;
    break;
  case INVTYPE_FINGER:
    if (nextFinger <= 11)
      slot = nextFinger++;
    break;
  case INVTYPE_TRINKET:
    if (nextTrinket <= 13)
      slot = nextTrinket++;
    break;
  case INVTYPE_CLOAK:
    slot = 14;
    break;
  case INVTYPE_WEAPON:
  case INVTYPE_2HWEAPON:
  case INVTYPE_WEAPONMAINHAND:
    slot = 15;
    break;
  case INVTYPE_SHIELD:
  case INVTYPE_WEAPONOFFHAND:
  case INVTYPE_HOLDABLE:
    slot = 16;
    break;
  case INVTYPE_RANGED:
  case INVTYPE_THROWN:
  case INVTYPE_RANGEDRIGHT:
    slot = 17;
    break;
  case INVTYPE_TABARD:
    slot = 18;
    break;
  default:
    return std::nullopt;
  }

  if (slot == 0xFF || slot >= EQUIPMENT_SLOT_END)
    return std::nullopt;
  if (occupied[slot])
    return std::nullopt;
  occupied[slot] = true;
  return slot;
}

} // namespace Firelands
