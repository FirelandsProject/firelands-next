#pragma once

#include <domain/models/PlayerCreateInfo.h>
#include <shared/game/AccessLevel.h>
#include <shared/game/Bag0InventoryData.h>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace Firelands {

class Character;

struct MailInboxItemRow {
  uint32_t itemGuidLow = 0;
  uint32_t itemEntry = 0;
  uint32_t count = 0;
};

struct MailInboxRow {
  uint64_t mailId = 0;
  uint32_t senderGuidLow = 0;
  std::string subject;
  std::string body;
  uint32_t checked = 0;
  uint32_t deliverTime = 0;
  uint32_t expireTime = 0;
  std::vector<MailInboxItemRow> items;
};

class ICharacterRepository {
public:
  virtual ~ICharacterRepository() = default;

  virtual std::vector<std::shared_ptr<Character>>
  GetCharactersByAccount(uint32_t accountId) = 0;
  virtual std::optional<uint32_t> CreateCharacter(const Character &character) = 0;
  virtual bool GrantStarterItems(uint32_t characterGuid,
                                 std::vector<StarterItemGrant> const &items) = 0;
  virtual bool DeleteCharacter(uint32_t guid, uint32_t accountId) = 0;
  virtual bool IsNameAvailable(const std::string &name) = 0;
  virtual std::optional<Character> GetCharacterByGuid(uint64_t guid) = 0;
  /// Exchange or move rows in `character_inventory` for bag 0 (equipment + main backpack grid).
  virtual bool SwapBag0Slots(uint32_t characterGuid, uint8_t srcSlot,
                           uint8_t dstSlot) = 0;

  /// Persists position/orientation and clears `firstLogin` after a world session.
  /// Must verify `accountId` so callers cannot update another account's character.
  virtual bool SaveCharacterOnLogout(uint32_t accountId, uint32_t characterGuid,
                                     uint16_t mapId, uint16_t zoneId, float x,
                                     float y, float z, float orientation,
                                     uint32_t moneyCopper) = 0;

  virtual bool UpdateCharacterMoney(uint32_t accountId, uint32_t characterGuid,
                                    uint32_t moneyCopper) = 0;
  virtual bool UpdateCharacterLevel(uint32_t accountId, uint32_t characterGuid,
                                    uint8_t level) = 0;
  virtual std::vector<uint32_t> GetCharacterSpellIds(uint32_t characterGuid) = 0;
  virtual bool AddCharacterSpell(uint32_t characterGuid, uint32_t spellId) = 0;
  /// True if `itemEntry` resolves from `item_template`, item DB2 hotfix, or CharStartOutfit DBC.
  virtual bool HasItemTemplate(uint32_t itemEntry) const = 0;
  /// Creates `item_instance` + `character_inventory` row in bag 0 (equipment or backpack).
  /// On success, optionally fills `outItemGuidLow` / `outBag0Slot` (backpack slot 23..38).
  virtual bool GrantItemToBag0(uint32_t characterGuid, uint32_t itemEntry, uint32_t count,
                               uint32_t *outItemGuidLow = nullptr,
                               uint8_t *outBag0Slot = nullptr) = 0;
  /// Creates `item_instance` + `mail` + `mail_items` (item not placed in inventory).
  virtual bool SendGmMailWithItem(uint32_t receiverCharacterGuid, uint32_t itemEntry,
                                  uint32_t count) = 0;
  /// Removes up to `count` matching items from backpack grid only; returns amount removed.
  virtual uint32_t RemoveBag0ItemsByEntry(uint32_t characterGuid, uint32_t itemEntry,
                                          uint32_t count) = 0;
  virtual AccessLevel GetAccountAccessLevel(uint32_t accountId) = 0;
  /// Move an item from backpack grid (`INVENTORY_SLOT_ITEM_*`) to its default equipment slot.
  virtual bool AutoEquipFromBag0Slot(
      uint32_t characterGuid, uint8_t srcSlot,
      std::optional<uint8_t> fallbackInventoryType = std::nullopt) = 0;
  /// Main backpack grid only (`INVENTORY_SLOT_ITEM_START`..`END`). `clientCount` 0 =
  /// remove entire stack; otherwise reduce stack by `min(clientCount, stack)`.
  virtual bool DestroyBag0BackpackItem(uint32_t characterGuid, uint8_t slot,
                                       uint32_t clientCount) = 0;
  virtual bool SaveInventory(uint32_t characterGuid,
                          Bag0InventoryData const &invData) = 0;

  /// Rows from `mail` / `mail_items` for the given character (receiver).
  virtual std::vector<MailInboxRow> LoadMailInbox(uint32_t receiverGuid) = 0;
};

} // namespace Firelands
