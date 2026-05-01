#pragma once

#include <shared/Common.h>
#include <cstddef>

namespace Firelands {

/// Matches Trinity/FirelandsCore equipment slots (bag 0).
enum EquipmentSlots : uint8 {
  EQUIPMENT_SLOT_HEAD = 0,
  EQUIPMENT_SLOT_END = 19,
};

enum InventoryPackSlots : uint8 {
  INVENTORY_SLOT_ITEM_START = 23,
  INVENTORY_SLOT_ITEM_END = 39,
};

constexpr size_t kEquipmentSlotCount =
    static_cast<size_t>(EQUIPMENT_SLOT_END);

} // namespace Firelands
