#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Firelands {

/// Gossip option icons (client-side visual).
enum class GossipOptionIcon : uint8_t {
  Chat = 0,
  Vendor = 1,
  Taxi = 2,
  Trainer = 3,
  Interact1 = 4,
  Interact2 = 5,
  MoneyBag = 6,
  Talk = 7,
  Tabard = 8,
  Battle = 9,
  Dot = 10,
  Chat11 = 11,
  Chat12 = 12,
  Chat13 = 13,
  Chat16 = 16,
  Chat17 = 17,
  Chat18 = 18,
  Chat19 = 19,
  Chat20 = 20,
  Chat21 = 21,
};

/// Gossip option types (server-side action dispatch).
enum class GossipOptionType : uint32_t {
  None = 0,
  Gossip = 1,
  QuestGiver = 2,
  Vendor = 3,
  TaxiVendor = 4,
  Trainer = 5,
  SpiritHealer = 6,
  SpiritGuide = 7,
  Innkeeper = 8,
  Banker = 9,
  Petitioner = 10,
  TabardDesigner = 11,
  Battlefield = 12,
  Auctioneer = 13,
  StablePet = 14,
  Armorer = 15,
  UnlearnTalents = 16,
  UnlearnPetTalents = 17,
  LearnDualSpec = 18,
  OutdoorPvp = 19,
  DualSpecInfo = 20,
};

/// A single gossip menu option.
struct GossipMenuItem {
  uint32_t menuId = 0;
  uint32_t optionIndex = 0;
  GossipOptionIcon icon = GossipOptionIcon::Chat;
  std::string optionText;
  uint32_t optionBroadcastTextId = 0;
  GossipOptionType optionType = GossipOptionType::None;
  uint64_t optionNpcflag = 0;
  uint16_t verifiedBuild = 0;
  bool isCoded = false;
  uint32_t boxMoney = 0;
  std::string boxMessage;
  uint32_t boxBroadcastTextId = 0;
  uint32_t actionMenuId = 0;
  uint32_t actionPoi = 0;
  uint32_t sender = 0;
  uint32_t action = 0;
};

/// A quest item shown alongside gossip options.
struct GossipQuestItem {
  uint32_t questId = 0;
  uint8_t questIcon = 0;
  int32_t questLevel = 0;
  uint32_t questFlags = 0;
  bool blueQuestionMark = false;
  std::string questTitle;
};

/// Gossip menu loaded from DB or built by scripts.
struct GossipMenu {
  uint32_t menuId = 0;
  uint32_t textId = 0;
  std::vector<GossipMenuItem> items;

  void Clear() {
    items.clear();
  }

  bool Empty() const {
    return items.empty();
  }

  uint32_t GetItemCount() const {
    return static_cast<uint32_t>(items.size());
  }

  const GossipMenuItem *GetItem(uint32_t optionIndex) const {
    for (auto const &item : items) {
      if (item.optionIndex == optionIndex)
        return &item;
    }
    return nullptr;
  }
};

} // namespace Firelands
