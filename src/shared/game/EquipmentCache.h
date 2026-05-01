#pragma once

#include <domain/models/PlayerCreateInfo.h>
#include <shared/Common.h>
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace Firelands::EquipmentCache {

struct VisualSlotData {
  uint8 invType = 0;
  uint32 displayId = 0;
  uint32 displayEnchantId = 0;
};

using VisualArray = std::array<VisualSlotData, 23>;

std::string Serialize(std::vector<PlayerCreateVisualItem> const &items);

VisualArray Parse(std::string const &serialized);

} // namespace Firelands::EquipmentCache
