#pragma once

#include <domain/models/PlayerCreateInfo.h>
#include <domain/repositories/ICharacterRepository.h>
#include <application/services/PlayerCreateInfoService.h>
#include <memory>
#include <shared/game/EquipmentCache.h>
#include <shared/Common.h>
#include <algorithm>
#include <optional>
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
      if (!grants.empty())
        m_repository->GrantStarterItems(*guid, grants);
    }
    return true;
  }

  bool DeleteCharacter(uint32_t guid, uint32_t accountId) {
    return m_repository->DeleteCharacter(guid, accountId);
  }

  std::optional<Character> GetCharacterByGuid(uint64_t guid) {
    return m_repository->GetCharacterByGuid(guid);
  }

  bool SwapBag0Slots(uint64_t characterGuid, uint8_t srcSlot, uint8_t dstSlot) {
    return m_repository->SwapBag0Slots(static_cast<uint32_t>(characterGuid),
                                       srcSlot, dstSlot);
  }

private:
  std::shared_ptr<ICharacterRepository> m_repository;
  std::shared_ptr<PlayerCreateInfoService> m_playerCreateInfoService;
};

} // namespace Firelands
