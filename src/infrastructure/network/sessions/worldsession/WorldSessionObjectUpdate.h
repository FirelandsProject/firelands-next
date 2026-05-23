#ifndef FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_WORLDSESSION_OBJECT_UPDATE_H
#define FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_WORLDSESSION_OBJECT_UPDATE_H

#include <domain/ports/IMapNotifier.h>
#include <domain/models/Character.h>
#include <domain/models/PlayerCreateInfo.h>
#include <domain/world/Creature.h>
#include <shared/game/PlayerGmAppearance.h>
#include <shared/network/MovementInfo.h>
#include <shared/network/WorldPacket.h>

#include <application/spell/PlayerAuraStatEffects.h>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Firelands {

class GtPlayerStatGameTables;

/// SMSG_QUERY_PLAYER_NAME_RESPONSE result byte (4.3.4).
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
    std::optional<std::pair<uint32, uint32>> const &healthOverride = std::nullopt,
    std::optional<std::pair<uint32, uint32>> const &power1Override = std::nullopt,
    std::vector<StarterSkillGrant> const &starterSkills = {});

/// `UNIT_FLAG_CAN_SWIM` + `UNIT_FIELD_BYTES_1` anim tier from movement (call after GM merge).
void ApplyMovementHintsToPlayerCreateFields(std::map<uint16, uint32> &fields,
                                            MovementInfo const &move);

/// Swim / fly anim tier + `UNIT_FLAG_CAN_SWIM` while moving (call after GM merge).
void BuildPlayerMovementHintsValuesUpdate(uint16 mapId, uint64 playerGuid,
                                          MovementInfo const &move,
                                          PlayerGmAppearanceForUpdates const &gmAppearance,
                                          WorldPacket &outPacket);

/// `SMSG_UPDATE_OBJECT` with `UPDATETYPE_VALUES` for `UNIT_FIELD_HEALTH` / max only.
void BuildPlayerHealthValuesUpdate(uint16 mapId, uint64 playerGuid, uint32 health,
                                   uint32 maxHealth, WorldPacket &outPacket);

void BuildPlayerPower1ValuesUpdate(uint16 mapId, uint64 playerGuid, uint32 power1,
                                   uint32 maxPower1, WorldPacket &outPacket);

/// `PLAYER_REST_STATE_EXPERIENCE` + `PLAYER_BYTES_2` rest icon (clear when pool is 0).
void BuildPlayerRestStateValuesUpdate(uint16 mapId, uint64 playerGuid, float restBonus,
                                      uint8_t facialHair, WorldPacket &outPacket);

void BuildPlayerActionBarTogglesValuesUpdate(uint16 mapId, uint64 playerGuid,
                                             uint8 actionBarToggles,
                                             WorldPacket &outPacket);

/// Aura-driven stat/rating/resistance/damage fields. When `baseline` is set, spell damage
/// and resistance buff mods include template values (login zeros those until this runs).
void BuildPlayerAuraStatValuesUpdate(uint16 mapId, uint64 playerGuid,
                                     PlayerAuraStatBonus const &bonus,
                                     WorldPacket &outPacket,
                                     UnitCombatStats const *baseline = nullptr,
                                     float baselineDodgePct = 0.f);

/// `SMSG_UPDATE_OBJECT` values block for `UNIT_FIELD_FACTIONTEMPLATE` only (players or units).
void BuildUnitFactionTemplateValuesUpdate(uint16 mapId, uint64 unitGuid,
                                          uint32 factionTemplate,
                                             WorldPacket &outPacket);

/// `SMSG_UPDATE_OBJECT` values block for `UNIT_NPC_EMOTESTATE` only.
void BuildUnitNpcEmoteStateValuesUpdate(uint16 mapId, uint64 unitGuid,
                                        uint32 emoteState, WorldPacket &outPacket);

/// `SMSG_UPDATE_OBJECT` values block for `UNIT_FIELD_TARGET` only (`targetGuid` 0 clears).
void BuildUnitTargetValuesUpdate(uint16 mapId, uint64 unitGuid, uint64 targetGuid,
                                             WorldPacket &outPacket);

void AppendPlayerGuidLookupData(WorldPacket &dst, Character const &ch,
                                std::string const &realmName);

uint64 ReadClientTargetGuid(WorldPacket &packet);

/// Packed `ObjectGuid` (quest giver status query/hello); not the 8-byte gossip target form.
uint64 ReadClientQuestGiverGuid(WorldPacket &packet);

void SendPlayerCreateToNotifier(
    std::shared_ptr<IMapNotifier> target, uint32 mapId, uint64 objectGuid,
    Character const &character, MovementInfo const &move,
    PlayerGmAppearanceForUpdates const &gmAppearance = {},
    GtPlayerStatGameTables const *statGameTables = nullptr,
    uint32_t nextLevelXpFromWorld = 0,
    std::optional<std::pair<uint32, uint32>> const &healthOverride = std::nullopt,
    std::optional<std::pair<uint32, uint32>> const &power1Override = std::nullopt);

/// Minimal `TYPEID_UNIT` field map for NPC creates; `factionTemplate` should match
/// `creature_template.faction` when that row exists.
/// \param npcFlags `UNIT_NPC_FLAGS` (`creature_template.npcflag`).
/// \param factionTemplate `FactionTemplate.dbc` id (`UNIT_FIELD_FACTIONTEMPLATE`).
/// \param unitFieldFlags `UNIT_FIELD_FLAGS` (`creature_template.unit_flags`).
/// \param unitFieldFlags2 `UNIT_FIELD_FLAGS_2` (`creature_template.unit_flags2`).
std::map<uint16, uint32> BuildMinimalNpcUnitCreateFields(uint64 objectGuid,
                                                         uint32 creatureEntry,
                                                         uint32 displayId,
                                                         uint32 health,
                                                         uint32 maxHealth,
                                                         uint8 level,
                                                         uint32 npcFlags = 0,
                                                         uint32 factionTemplate =
                                                             Creature::kDefaultFactionTemplate,
                                                         uint32 unitFieldFlags = 0,
                                                         uint32 unitFieldFlags2 = 0);

                                                             /// `SMSG_CREATURE_QUERY_RESPONSE` (`QueryCreatureResponse::Write`, 4.3.4).
/// If `nameTitle` is empty, sends entry with high bit set (client shows no template).
void BuildCreatureQueryResponse(
    WorldPacket &out, uint32 creatureEntry,
    std::optional<std::pair<std::string, std::string>> const &nameTitle,
    std::array<uint32, 4> const &creatureDisplayIds = {});

} // namespace WorldSessionObjectUpdate

} // namespace Firelands

#endif
