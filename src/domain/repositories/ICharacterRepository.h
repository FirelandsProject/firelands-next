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
  /// Creates `item_instance` + `character_inventory` row in bag 0 (equipment or backpack).
  virtual bool GrantItemToBag0(uint32_t characterGuid, uint32_t itemEntry,
                               uint32_t count) = 0;
  virtual AccessLevel GetAccountAccessLevel(uint32_t accountId) = 0;
  /// Move an item from backpack grid (`INVENTORY_SLOT_ITEM_*`) to its default equipment slot.
  virtual bool AutoEquipFromBag0Slot(
      uint32_t characterGuid, uint8_t srcSlot,
      std::optional<uint8_t> fallbackInventoryType = std::nullopt) = 0;
  virtual bool SaveInventory(uint32_t characterGuid,
                          Bag0InventoryData const &invData) = 0;
};

} // namespace Firelands
