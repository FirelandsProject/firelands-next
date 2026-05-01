#pragma once

#include <domain/models/PlayerCreateInfo.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace Firelands {

/// Loads CharStartOutfit.dbc using the real FirelandsCore field layout (`DBCfmt.h`).
class CharStartOutfitDbc {
public:
  bool Load(std::string const &dbcPath);

  /// Character screen / equipment-cache visuals (display ids + inventory types).
  std::vector<PlayerCreateVisualItem>
  GetVisualItems(uint8 race, uint8 klass, uint8 gender) const;

  /// Item ids for starter inventory (reference `Player::Create` CharStartOutfit block).
  std::vector<uint32_t> GetStarterItemIds(uint8 race, uint8 klass,
                                          uint8 gender) const;

private:
  using OutfitKey = uint32;

  static OutfitKey MakeKey(uint8 race, uint8 klass, uint8 gender) {
    return static_cast<uint32>(race) | (static_cast<uint32>(klass) << 8) |
           (static_cast<uint32>(gender) << 16);
  }

  std::unordered_map<OutfitKey, std::vector<PlayerCreateVisualItem>> m_visuals;
  std::unordered_map<OutfitKey, std::vector<uint32_t>> m_itemIds;
};

} // namespace Firelands
