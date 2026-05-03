#ifndef FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_WORLDSESSION_OBJECT_UPDATE_H
#define FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_WORLDSESSION_OBJECT_UPDATE_H

#include <application/ports/IMapNotifier.h>
#include <domain/models/Character.h>
#include <shared/game/PlayerGmAppearance.h>
#include <shared/network/MovementInfo.h>
#include <shared/network/WorldPacket.h>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Firelands {

class GtPlayerStatGameTables;

/// SMSG_QUERY_PLAYER_NAME_RESPONSE result byte (TCPP / 4.3.4).
enum QueryNameResponseCode : uint8 {
  kQueryNameResponseSuccess = 0,
  kQueryNameResponseFailure = 1,
};

namespace WorldSessionObjectUpdate {

std::vector<uint32> BuildDefaultKnownSpells(uint8 classId);

std::map<uint16, uint32> BuildItemCreateFields(uint64 itemObjectGuid,
                                               uint64 ownerGuid, uint32 itemEntry,
                                               uint32 stackCount);

std::map<uint16, uint32> BuildPlayerBag0InventoryValues(Character const &character);

std::map<uint16, uint32> BuildPlayerUpdateFields(
    uint64 guid, Character const &character,
    GtPlayerStatGameTables const *statGameTables = nullptr,
    uint32_t nextLevelXpFromWorld = 0,
    std::optional<std::pair<uint32, uint32>> const &healthOverride = std::nullopt);

/// `SMSG_UPDATE_OBJECT` with `UPDATETYPE_VALUES` for `UNIT_FIELD_HEALTH` / max only.
void BuildPlayerHealthValuesUpdate(uint16 mapId, uint64 playerGuid, uint32 health,
                                   uint32 maxHealth, WorldPacket &outPacket);

void AppendPlayerGuidLookupData(WorldPacket &dst, Character const &ch,
                                std::string const &realmName);

uint64 ReadClientTargetGuid(WorldPacket &packet);

void SendPlayerCreateToNotifier(
    std::shared_ptr<IMapNotifier> target, uint32 mapId, uint64 objectGuid,
    Character const &character, MovementInfo const &move,
    PlayerGmAppearanceForUpdates const &gmAppearance = {},
    GtPlayerStatGameTables const *statGameTables = nullptr,
    uint32_t nextLevelXpFromWorld = 0,
    std::optional<std::pair<uint32, uint32>> const &healthOverride = std::nullopt);

} // namespace WorldSessionObjectUpdate

} // namespace Firelands

#endif
