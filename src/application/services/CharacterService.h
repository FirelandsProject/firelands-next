#pragma once

#include <domain/models/Character.h>
#include <domain/models/PlayerCreateInfo.h>
#include <domain/repositories/ICharacterRepository.h>
#include <application/services/PlayerCreateInfoService.h>
#include <cstdint>
#include <memory>
#include <shared/game/EquipmentCache.h>
#include <shared/game/AccessLevel.h>
#include <shared/game/InventorySlots.h>
#include <shared/game/ItemEquipSlots.h>
#include <shared/Common.h>
#include <algorithm>
#include <optional>
#include <unordered_set>
#include <vector>

namespace Firelands {

namespace {

/// Used when `firelands_world.playercreateinfo` is missing/outdated (common on fresh Docker DB).
inline std::optional<PlayerCreateInfo> FallbackStartPosition(uint8 race) {
  switch (race) {
  case 1: // Human — Northshire
    return PlayerCreateInfo{0, 9, -8914.57f, -133.909f, 80.5378f, 5.13806f};
  case 2: // Orc — Valley of Trials
    return PlayerCreateInfo{1, 14, -618.518f, -4251.67f, 38.718f, 4.72222f};
  case 3: // Dwarf — Coldridge
    return PlayerCreateInfo{0, 1, -6240.32f, 331.033f, 382.758f, 6.17716f};
  case 4: // Night Elf — Shadowglen
    return PlayerCreateInfo{1, 141, 10311.3f, 832.463f, 1326.41f, 5.69632f};
  case 5: // Undead — Deathknell
    return PlayerCreateInfo{0, 5692, 1699.85f, 1706.56f, 135.928f, 4.88839f};
  case 6: // Tauren — Camp Narache
    return PlayerCreateInfo{1, 221, -2915.55f, -257.347f, 59.2693f, 0.302378f};
  case 7: // Gnome — Coldridge / Gnomergan exit
    return PlayerCreateInfo{0, 5495, -4983.42f, 877.7f, 274.31f, 3.06393f};
  case 8: // Troll — Echo Isles / Durotar
    return PlayerCreateInfo{1, 5691, -1171.45f, -5263.65f, 0.847728f,
                             5.78945f};
  case 9: // Goblin
    return PlayerCreateInfo{648, 4765, -8423.81f, 1361.3f, 104.671f, 1.55428f};
  case 10: // Blood Elf
    return PlayerCreateInfo{530, 3431, 10349.6f, -6357.29f, 33.4026f, 5.31605f};
  case 11: // Draenei
    return PlayerCreateInfo{530, 3526, -3961.64f, -13931.2f, 100.615f,
                             2.08364f};
  case 22: // Worgen — Gilneas phase area (reference starter row)
    return PlayerCreateInfo{654, 4756, -1451.53f, 1403.35f, 35.5561f,
                             0.333847f};
  default:
    return std::nullopt;
  }
}

inline void AppendGmStarterItems(std::vector<StarterItemGrant> &grants) {
  // GM visual outfit only (no weapon — avoids class weapon restrictions and sparse/tooltip
  // mismatches for hotfix-only item entries).
  struct GmStarterItem {
    uint32_t itemId;
    uint8_t invType;
  };
  static constexpr GmStarterItem kGmItems[] = {
      {12064u, INVTYPE_HEAD},   // Gamemaster Hood
      {2586u, INVTYPE_ROBE},    // Gamemaster's Robe (chest slot)
      {11508u, INVTYPE_FEET},   // Gamemaster's Slippers
  };
  std::unordered_set<uint32_t> existing;
  existing.reserve(grants.size());
  for (StarterItemGrant const &g : grants) {
    if (g.itemId != 0)
      existing.insert(g.itemId);
  }
  for (GmStarterItem const &item : kGmItems) {
    if (existing.insert(item.itemId).second) {
      StarterItemGrant g;
      g.itemId = item.itemId;
      g.count = 1u;
      g.invType = item.invType;
      g.bagOnly = true;
      grants.push_back(g);
    }
  }
}

} // namespace

class CharacterService {
public:
  explicit CharacterService(
      std::shared_ptr<ICharacterRepository> repository,
      std::shared_ptr<PlayerCreateInfoService> playerCreateInfoService = nullptr)
      : m_repository(std::move(repository)),
        m_playerCreateInfoService(std::move(playerCreateInfoService)) {}

  std::vector<std::shared_ptr<Character>>
  GetCharactersForAccount(uint32 accountId) {
    return m_repository->GetCharactersByAccount(accountId);
  }

  std::vector<uint32_t> GetStarterSpells(uint8_t race, uint8_t klass) const {
    if (!m_playerCreateInfoService)
      return {};
    return m_playerCreateInfoService->GetStarterSpells(race, klass);
  }

  bool CreateCharacter(uint32 accountId, std::string name, uint8 race,
                       uint8 klass, uint8 gender, uint8 skin, uint8 face,
                       uint8 hairStyle, uint8 hairColor, uint8 facialHair,
                       uint8 outfitId = 0) {

    if (!m_repository->IsNameAvailable(name)) {
      return false;
    }

    uint16 mapId = 0;
    uint16 zoneId = 12;
    float x = -8949.95f, y = -132.49f, z = 83.53f, o = 0.0f;

    std::optional<PlayerCreateInfo> startPos;
    if (m_playerCreateInfoService)
      startPos = m_playerCreateInfoService->GetStartPosition(race, klass);
    if (!startPos)
      startPos = FallbackStartPosition(race);
    if (startPos) {
      mapId = startPos->mapId;
      zoneId = static_cast<uint16>(
          std::min<uint32_t>(startPos->zoneId, 65535u));
      x = startPos->x;
      y = startPos->y;
      z = startPos->z;
      o = startPos->orientation;
    }

    std::string equipmentCache;
    if (m_playerCreateInfoService) {
      auto visualItems =
          m_playerCreateInfoService->GetVisualItems(race, klass, gender, outfitId);
      equipmentCache = EquipmentCache::Serialize(visualItems);
    }

    Character newChar(0, accountId, name, race, klass, gender, skin, face,
                      hairStyle, hairColor, facialHair, 1, zoneId, mapId, x, y,
                      z, o, 0, 0, 0, true, outfitId, equipmentCache);

    auto guid = m_repository->CreateCharacter(newChar);
    if (!guid)
      return false;

    if (m_playerCreateInfoService) {
      auto grants = m_playerCreateInfoService->GetStarterItemGrants(
          race, klass, gender, outfitId);
      if (HasAtLeast(m_repository->GetAccountAccessLevel(accountId),
                     AccessLevel::GameMaster)) {
        AppendGmStarterItems(grants);
      }
      if (!grants.empty())
        m_repository->GrantStarterItems(*guid, grants);
    }
    return true;
  }

  bool DeleteCharacter(uint32_t guid, uint32_t accountId) {
    return m_repository->DeleteCharacter(guid, accountId);
  }

  std::optional<Character> GetCharacterByGuid(uint64_t guid) {
    auto c = m_repository->GetCharacterByGuid(guid);
    if (c && m_playerCreateInfoService)
      m_playerCreateInfoService->TryApplyTemplateCombatState(*c);
    return c;
  }

  bool SwapBag0Slots(uint64_t characterGuid, uint8_t srcSlot, uint8_t dstSlot) {
    return m_repository->SwapBag0Slots(static_cast<uint32_t>(characterGuid),
                                       srcSlot, dstSlot);
  }

  bool SaveCharacterOnLogout(uint32_t accountId, uint32_t characterGuid,
                             uint16_t mapId, uint16_t zoneId, float x, float y,
                             float z, float orientation, uint32_t moneyCopper) {
    return m_repository->SaveCharacterOnLogout(accountId, characterGuid, mapId,
                                               zoneId, x, y, z, orientation,
                                               moneyCopper);
  }

bool UpdateCharacterMoney(uint32_t accountId, uint32_t characterGuid,
                            uint32_t moneyCopper) {
    return m_repository->UpdateCharacterMoney(accountId, characterGuid,
                                            moneyCopper);
  }

  bool SaveInventory(uint32_t characterGuid, Bag0InventoryData const &invData) {
    return m_repository->SaveInventory(characterGuid, invData);
  }

  bool AddCharacterMoneyDelta(uint32_t accountId, uint32_t characterGuid,
                              int64 deltaCopper) {
    auto ch = m_repository->GetCharacterByGuid(characterGuid);
    if (!ch || ch->GetAccount() != accountId)
      return false;
    int64 const base = static_cast<int64>(ch->GetMoney());
    int64 sum = base + deltaCopper;
    if (sum < 0)
      sum = 0;
    constexpr int64 kMaxMoney = static_cast<int64>(0xFFFFFFFFu);
    if (sum > kMaxMoney)
      sum = kMaxMoney;
    return m_repository->UpdateCharacterMoney(accountId, characterGuid,
                                                static_cast<uint32_t>(sum));
  }

  bool SetCharacterLevel(uint32_t accountId, uint32_t characterGuid,
                         uint8_t level) {
    return m_repository->UpdateCharacterLevel(accountId, characterGuid, level);
  }

  std::vector<uint32_t> GetCharacterSpellIds(uint32_t characterGuid) {
    return m_repository->GetCharacterSpellIds(characterGuid);
  }

  bool AddCharacterSpell(uint32_t characterGuid, uint32_t spellId) {
    return m_repository->AddCharacterSpell(characterGuid, spellId);
  }

  bool GrantItemToBag0(uint32_t characterGuid, uint32_t itemEntry,
                       uint32_t count) {
    return m_repository->GrantItemToBag0(characterGuid, itemEntry, count);
  }

  bool AutoEquipFromBag0(uint32_t accountId, uint32_t characterGuid, uint8_t bag,
                         uint8_t slot) {
    uint8_t const normalizedBag =
        (bag == CLIENT_INVENTORY_SLOT_DEFAULT_BACKPACK) ? 0 : bag;
    if (normalizedBag != 0)
      return false;
    auto ch = m_repository->GetCharacterByGuid(characterGuid);
    if (!ch || ch->GetAccount() != accountId)
      return false;

    std::optional<uint8_t> fallbackInventoryType = std::nullopt;
    if (m_playerCreateInfoService && slot >= INVENTORY_SLOT_ITEM_START &&
        slot < INVENTORY_SLOT_ITEM_END) {
      size_t const packIndex =
          static_cast<size_t>(slot - INVENTORY_SLOT_ITEM_START);
      uint32_t const entry = ch->GetPackItemEntry(packIndex);
      if (entry != 0) {
        auto grants = m_playerCreateInfoService->GetStarterItemGrants(
            ch->GetRace(), ch->GetClass(), ch->GetGender(), ch->GetOutfitId());
        if (HasAtLeast(m_repository->GetAccountAccessLevel(accountId),
                       AccessLevel::GameMaster)) {
          AppendGmStarterItems(grants);
        }
        for (StarterItemGrant const &grant : grants) {
          if (grant.itemId == entry && grant.invType != 0) {
            fallbackInventoryType = grant.invType;
            break;
          }
        }
      }
    }

    return m_repository->AutoEquipFromBag0Slot(characterGuid, slot,
                                               fallbackInventoryType);
  }

  /// Client `gtCombatRatings` / `gtChanceTo*Crit*` tables (may be unloaded if DBC path empty).
  GtPlayerStatGameTables const *GetStatGameTables() const {
    return m_playerCreateInfoService ? &m_playerCreateInfoService->GetStatGameTables()
                                     : nullptr;
  }

private:
  std::shared_ptr<ICharacterRepository> m_repository;
  std::shared_ptr<PlayerCreateInfoService> m_playerCreateInfoService;
};

} // namespace Firelands
