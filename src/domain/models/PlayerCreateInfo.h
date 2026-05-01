#pragma once

#include <shared/Common.h>
#include <vector>

namespace Firelands {

struct PlayerCreateInfo {
  uint16 mapId = 0;
  uint32 zoneId = 0;
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float orientation = 0.0f;
};

/// Starter items from CharStartOutfit / playercreateinfo_item (reference: Player::StoreNewItemInBestSlots).
struct StarterItemGrant {
  uint32 itemId = 0;
  uint32 count = 1;
};

struct PlayerCreateVisualItem {
  uint8 slot = 0;
  uint8 invType = 0;
  uint32 displayId = 0;
  uint32 displayEnchantId = 0;
};

} // namespace Firelands
