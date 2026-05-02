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

/// Client inventory position for the default backpack (TCPP `INVENTORY_SLOT_BAG_0`;
/// DB `character_inventory.bag` is `0`).
constexpr uint8_t CLIENT_INVENTORY_SLOT_DEFAULT_BACKPACK = 255;

constexpr size_t kEquipmentSlotCount =
    static_cast<size_t>(EQUIPMENT_SLOT_END);

/// Main backpack grid slots (bag 0): 23 .. 38 inclusive.
constexpr size_t kPackSlotCount = static_cast<size_t>(
    INVENTORY_SLOT_ITEM_END - INVENTORY_SLOT_ITEM_START);

} // namespace Firelands
