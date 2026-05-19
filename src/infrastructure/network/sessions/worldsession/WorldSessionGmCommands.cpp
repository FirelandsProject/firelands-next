#include <application/logic/CreatureSpawnLogic.h>
#include <application/services/WorldService.h>
#include <domain/repositories/INpcTemplateSearchRepository.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <domain/world/Creature.h>
#include <domain/world/Player.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/dbc/FactionTemplateDbc.h>
#include <shared/game/ChatLanguages.h>
#include <shared/game/InventorySlots.h>
#include <shared/game/ReputationRank.h>
#include <shared/game/WowGuid.h>
#include <shared/network/UpdateData.h>
#include <shared/network/UpdateFields.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <shared/Logger.h>
#include <algorithm>
#include <atomic>
#include <limits>
#include <map>
#include <memory>

namespace Firelands {

namespace {

namespace ws_obj = WorldSessionObjectUpdate;

std::atomic<uint32_t> g_nextGmSpawnCreatureLow{0x70000000u};

/// Matches `CommandService` default when `.npc add` omits displayId.
constexpr uint32_t kGmNpcPlaceholderDisplayId = 15688u;

uint64_t AllocateGmSpawnCreatureGuid(uint32_t creatureEntry) {
  uint32_t const low =
      g_nextGmSpawnCreatureLow.fetch_add(1u, std::memory_order_relaxed);
  return MakeCreatureObjectGuid(creatureEntry, low);
}

void SendGmCreatureCreateVisibility(WorldSession &session, uint32 mapId,
                                    uint64 creatureGuid, MovementInfo const &move,
                                    uint32 entry, uint32 displayId, uint32 hp,
                                    uint32 maxHp, uint8 level, uint32 npcFlags = 0,
                                    uint32 factionTemplate =
                                        Creature::kDefaultFactionTemplate) {
  auto fields = WorldSessionObjectUpdate::BuildMinimalNpcUnitCreateFields(
      creatureGuid, entry, displayId, hp, maxHp, level, npcFlags,
      factionTemplate);
  UpdateData update(static_cast<uint16>(mapId));
  update.AddCreateObject(creatureGuid, TYPEID_UNIT, move, fields);
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  // Always notify the spawner (login uses the same pattern; creature-centered
  // BroadcastPacketToNearby can miss the initiating session).
  session.SendPacket(pkt);
  if (auto map = WorldService::Instance().GetMap(mapId))
    map->BroadcastPacketToNearby(session.GetGuid(), pkt, false);
}

void SendGmCreatureDespawnVisibility(WorldSession &session, uint32 mapId,
                                     uint64 creatureGuid) {
  UpdateData update(static_cast<uint16>(mapId));
  update.AddOutOfRangeObjects({creatureGuid});
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  session.SendPacket(pkt);
  if (auto map = WorldService::Instance().GetMap(mapId))
    map->BroadcastPacketToNearby(session.GetGuid(), pkt, false);
}

void BroadcastUnitHealthOnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                              uint64 unitGuid, uint32 health, uint32 maxHealth) {
  WorldPacket hpUpdate;
  ws_obj::BuildPlayerHealthValuesUpdate(static_cast<uint16>(mapId), unitGuid, health,
                                        maxHealth, hpUpdate);
  map->BroadcastPacketToNearby(unitGuid, hpUpdate, true);
}

} // namespace

void WorldSession::PublishSelfCoinageUpdate() {
  if (_playerGuid == 0)
    return;
  uint64 const coin = _moneyCopper;
  std::map<uint16, uint32> f;
  f[PLAYER_FIELD_COINAGE] = static_cast<uint32>(coin & 0xFFFFFFFFu);
  f[static_cast<uint16>(PLAYER_FIELD_COINAGE + 1)] =
      static_cast<uint32>((coin >> 32) & 0xFFFFFFFFu);
  UpdateData update(_mapId);
  update.AddValuesUpdate(_playerGuid, f);
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  SendPacket(pkt);
}

bool WorldSession::GmLearnSpell(uint32 spellId) {
  if (_playerGuid == 0 || spellId == 0)
    return false;
  if (_knownSpellIds.count(spellId) != 0u) {
    SendNotification("Already knows spell " + std::to_string(spellId) + ".");
    return true;
  }
  if (_spellDefinitions && !IsLanguagePassiveSpell(spellId) &&
      !_spellDefinitions->HasSpell(spellId)) {
    SendNotification("Spell id not in Spell.dbc (refused).");
    return false;
  }
  uint32 const lowGuid = static_cast<uint32>(_playerGuid);
  if (!_charService->AddCharacterSpell(lowGuid, spellId)) {
    SendNotification("Failed to persist spell.");
    return false;
  }
  _knownSpells.push_back(spellId);
  _knownSpellIds.insert(spellId);
  SendLearnedSpell(spellId);
  SendNotification("Learned spell " + std::to_string(spellId) + ".");
  return true;
}

bool WorldSession::GmSpawnNpc(uint32 creatureEntry, uint32 displayId,
                              uint32 factionTemplateOrZeroDefault) {
  if (_playerGuid == 0 || creatureEntry == 0 || displayId == 0)
    return false;

  uint32 resolvedDisplay = displayId;
  uint32 factionForCreature = factionTemplateOrZeroDefault;
  uint32 npcFlagsForSpawn = 0;
  if (_npcTemplateSearch) {
    if (auto const tpl = _npcTemplateSearch->TryGetByEntry(creatureEntry)) {
      if (displayId == kGmNpcPlaceholderDisplayId) {
        resolvedDisplay = ResolveCreatureDisplayId(
            0, tpl->displayIds[0], tpl->displayIds[1], tpl->displayIds[2],
            tpl->displayIds[3]);
      }
      if (factionForCreature == 0 && tpl->factionTemplate != 0)
        factionForCreature = tpl->factionTemplate;
      npcFlagsForSpawn = static_cast<uint32_t>(tpl->npcFlags);
    }
  }
  if (factionForCreature != 0 && _factionTemplateDbc &&
      _factionTemplateDbc->IsLoaded() &&
      !_factionTemplateDbc->HasEntry(factionForCreature)) {
    LOG_WARN("GM spawn entry {} faction {} not in FactionTemplate.dbc; using fallback",
             creatureEntry, factionForCreature);
    factionForCreature = 0;
  }

  uint64 const guid = AllocateGmSpawnCreatureGuid(creatureEntry);
  constexpr uint32_t kSeedHp = 100u;
  auto spawned = std::make_shared<Creature>(guid, creatureEntry, resolvedDisplay, kSeedHp,
                                            1u, factionForCreature, npcFlagsForSpawn);
  spawned->SetPosition(_position);
  WorldService::Instance().AddCreatureToMap(_mapId, std::move(spawned));

  auto map = WorldService::Instance().GetMap(_mapId);
  auto cr = map->TryGetCreature(guid);
  if (!cr)
    return false;

  SendGmCreatureCreateVisibility(*this, _mapId, guid, cr->GetPosition(), creatureEntry,
                                 resolvedDisplay, cr->GetLiveHealth(),
                                 cr->GetLiveMaxHealth(), cr->GetLevel(), npcFlagsForSpawn,
                                 cr->GetFactionTemplate());
  SendNotification("Spawned NPC entry=" + std::to_string(creatureEntry) +
                   " display=" + std::to_string(resolvedDisplay) + " guid=" +
                   std::to_string(guid) +
                   " factionTemplate=" + std::to_string(cr->GetFactionTemplate()) + ".");
  return true;
}

void WorldSession::PublishUnitFactionTemplateUpdate(uint64 unitGuid,
                                                    uint32 factionTemplate) {
  if (unitGuid == 0)
    return;
  WorldPacket pkt;
  WorldSessionObjectUpdate::BuildUnitFactionTemplateValuesUpdate(
      static_cast<uint16>(_mapId), unitGuid, factionTemplate, pkt);
  if (auto map = WorldService::Instance().GetMap(_mapId))
    map->BroadcastPacketToNearby(unitGuid, pkt, true);
}

bool WorldSession::GmSetForcedFactionReaction(uint32 factionDbcId,
                                              uint8 reputationRank) {
  if (_playerGuid == 0)
    return false;
  if (factionDbcId == 0) {
    SendNotification("faction id must be non-zero (Faction.dbc id).");
    return false;
  }
  if (!IsValidReputationRankValue(static_cast<uint32_t>(reputationRank))) {
    SendNotification("reputationRank must be 0..7 (hated..exalted).");
    return false;
  }
  _forcedFactionReactions[factionDbcId] = static_cast<uint32_t>(reputationRank);
  SendForcedReactions();
  return true;
}

bool WorldSession::GmClearForcedFactionReaction(uint32 factionDbcId) {
  if (_playerGuid == 0)
    return false;
  _forcedFactionReactions.erase(factionDbcId);
  SendForcedReactions();
  return true;
}

bool WorldSession::GmClearAllForcedFactionReactions() {
  if (_playerGuid == 0)
    return false;
  _forcedFactionReactions.clear();
  SendForcedReactions();
  return true;
}

bool WorldSession::GmSetOwnFactionTemplate(uint32 factionTemplate) {
  if (_playerGuid == 0)
    return false;
  if (factionTemplate == 0) {
    SendNotification("factionTemplate must be non-zero.");
    return false;
  }
  if (_factionTemplateDbc && _factionTemplateDbc->IsLoaded() &&
      !_factionTemplateDbc->HasEntry(factionTemplate)) {
    SendNotification("factionTemplate id not found in FactionTemplate.dbc.");
    return false;
  }
  auto map = WorldService::Instance().GetMap(_mapId);
  if (!map)
    return false;
  auto pl = map->TryGetPlayer(_playerGuid);
  if (!pl)
    return false;
  pl->SetFactionTemplate(factionTemplate);
  PublishUnitFactionTemplateUpdate(_playerGuid, factionTemplate);
  return true;
}

bool WorldSession::GmSetSelectedCreatureFactionTemplate(uint32 factionTemplate) {
  if (_playerGuid == 0)
    return false;
  if (_clientSelectionGuid == 0) {
    SendNotification("Select a creature first (target NPC).");
    return false;
  }
  if (factionTemplate == 0) {
    SendNotification("factionTemplate must be non-zero.");
    return false;
  }
  if (_factionTemplateDbc && _factionTemplateDbc->IsLoaded() &&
      !_factionTemplateDbc->HasEntry(factionTemplate)) {
    SendNotification("factionTemplate id not found in FactionTemplate.dbc.");
    return false;
  }
  auto map = WorldService::Instance().GetMap(_mapId);
  if (!map)
    return false;
  auto cr = map->TryGetCreature(_clientSelectionGuid);
  if (!cr) {
    SendNotification("Selection is not a creature on this map.");
    return false;
  }
  cr->SetFactionTemplate(factionTemplate);
  PublishUnitFactionTemplateUpdate(_clientSelectionGuid, cr->GetFactionTemplate());
  return true;
}

bool WorldSession::GmDeleteNpcByObjectGuid(uint64 objectGuid) {
  if (_playerGuid == 0 || objectGuid == 0)
    return false;
  if (objectGuid == _playerGuid) {
    SendNotification("Cannot delete your own player with .npc del.");
    return false;
  }

  auto map = WorldService::Instance().GetMap(_mapId);
  if (!map->TryGetCreature(objectGuid)) {
    SendNotification(
        "That target is not a creature on your current map (guid mismatch?).");
    return false;
  }

  SendGmCreatureDespawnVisibility(*this, _mapId, objectGuid);
  map->RemoveObject(objectGuid);
  SendNotification("Removed NPC guid=" + std::to_string(objectGuid) + ".");
  return true;
}

bool WorldSession::GmModifyMoneyCopper(int64 deltaCopper) {
  if (_playerGuid == 0)
    return false;
  uint32 const lowGuid = static_cast<uint32>(_playerGuid);
  if (!_charService->AddCharacterMoneyDelta(_accountId, lowGuid, deltaCopper))
    return false;
  if (auto ch = _charService->GetCharacterByGuid(_playerGuid))
    _moneyCopper = ch->GetMoney();
  PublishSelfCoinageUpdate();
  SendNotification("Money updated (copper=" + std::to_string(_moneyCopper) + ").");
  return true;
}

bool WorldSession::GmAddItem(uint32 itemEntry, uint32 count) {
  if (_playerGuid == 0)
    return false;
  uint32 const c = std::max(1u, count);
  if (!_charService->HasItemTemplate(itemEntry)) {
    SendNotification("Item does not exist (no template for entry " +
                     std::to_string(itemEntry) + ").");
    return false;
  }
  bool mailed = false;
  uint32_t newItemGuidLow = 0;
  uint8_t newBag0Slot = 0;
  if (!_charService->GrantItemToBag0OrMail(static_cast<uint32>(_playerGuid),
                                           itemEntry, c, &mailed, &newItemGuidLow,
                                           &newBag0Slot)) {
    SendNotification("Could not add item (backpack full and mail failed, or database "
                     "error).");
    return false;
  }
  if (!mailed) {
    auto refreshed = _charService->GetCharacterByGuid(_playerGuid);
    if (!refreshed)
      return false;
    uint32_t stackShown = c;
    if (newBag0Slot >= INVENTORY_SLOT_ITEM_START &&
        newBag0Slot < INVENTORY_SLOT_ITEM_END) {
      size_t const pi =
          static_cast<size_t>(newBag0Slot - INVENTORY_SLOT_ITEM_START);
      if (pi < kPackSlotCount)
        stackShown = refreshed->GetPackItemStackCount(pi);
    }
    UpdateData update(_mapId);
    MovementInfo itemMove{};
    uint64 const itemOg = MakeItemObjectGuid(newItemGuidLow);
    update.AddCreateObject(itemOg, TYPEID_ITEM, itemMove,
                           ws_obj::BuildItemCreateFields(itemOg, _playerGuid, itemEntry,
                                                         stackShown));
    update.AddValuesUpdate(_playerGuid,
                           ws_obj::BuildPlayerBag0InventoryValues(*refreshed));
    WorldPacket pkt(SMSG_UPDATE_OBJECT);
    update.Build(pkt);
    SendPacket(pkt);
    SendNotification("Item " + std::to_string(itemEntry) + " x" + std::to_string(c) +
                     " added to backpack.");
  } else {
    SendNotification("Backpack full; item " + std::to_string(itemEntry) + " x" +
                     std::to_string(c) +
                     " was sent to your mailbox (retrieve at a mailbox when mail is "
                     "enabled on the client).");
  }
  return true;
}

bool WorldSession::GmRemoveItem(uint32 itemEntry, uint32 count) {
  if (_playerGuid == 0)
    return false;
  uint32 const c = std::max(1u, count);
  uint32_t const removed = _charService->RemoveBag0ItemsByEntry(
      _accountId, static_cast<uint32>(_playerGuid), itemEntry, c);
  if (removed == 0) {
    SendNotification("No matching items in the main backpack (bag slots only).");
    return false;
  }
  auto refreshed = _charService->GetCharacterByGuid(_playerGuid);
  if (!refreshed)
    return false;
  UpdateData update(_mapId);
  update.AddValuesUpdate(_playerGuid,
                         ws_obj::BuildPlayerBag0InventoryValues(*refreshed));
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  SendPacket(pkt);
  SendNotification("Removed item " + std::to_string(itemEntry) + " x" +
                   std::to_string(removed) + " from backpack.");
  return true;
}

bool WorldSession::GmDamageUnit(uint64 targetGuid, uint32 amount) {
  if (_playerGuid == 0 || targetGuid == 0 || amount == 0)
    return false;

  uint32 capped = amount;
  if (capped > static_cast<uint32>(std::numeric_limits<int32_t>::max()))
    capped = static_cast<uint32>(std::numeric_limits<int32_t>::max());
  int32 const delta = -static_cast<int32>(capped);

  auto map = WorldService::Instance().GetMap(_mapId);
  if (!map)
    return false;

  if (auto player = map->TryGetPlayer(targetGuid)) {
    player->ApplyHealthDelta(delta);
    BroadcastUnitHealthOnMap(_mapId, map, targetGuid, player->GetLiveHealth(),
                             player->GetLiveMaxHealth());
    SendNotification("Dealt " + std::to_string(capped) + " damage to player guid=" +
                     std::to_string(targetGuid) + " (hp=" +
                     std::to_string(player->GetLiveHealth()) + "/" +
                     std::to_string(player->GetLiveMaxHealth()) + ").");
    return true;
  }

  if (auto creature = map->TryGetCreature(targetGuid)) {
    creature->ApplyHealthDelta(delta);
    BroadcastUnitHealthOnMap(_mapId, map, targetGuid, creature->GetLiveHealth(),
                             creature->GetLiveMaxHealth());
    SendNotification("Dealt " + std::to_string(capped) + " damage to creature guid=" +
                     std::to_string(targetGuid) + " (hp=" +
                     std::to_string(creature->GetLiveHealth()) + "/" +
                     std::to_string(creature->GetLiveMaxHealth()) + ").");
    return true;
  }

  return false;
}

bool WorldSession::GmSetLevel(uint8 level) {
  if (_playerGuid == 0)
    return false;
  uint8 lv = level;
  if (lv < 1)
    lv = 1;
  if (lv > 85)
    lv = 85;
  if (!_charService->SetCharacterLevel(_accountId, static_cast<uint32>(_playerGuid),
                                       lv)) {
    SendNotification("Failed to set level.");
    return false;
  }
  _playerXp = 0;
  std::map<uint16, uint32> f;
  f[UNIT_FIELD_LEVEL] = lv;
  if (lv < 85) {
    uint32_t const next = _charService->GetXpToNextLevelForLevel(lv);
    f[PLAYER_XP] = 0;
    f[PLAYER_NEXT_LEVEL_XP] = next;
  } else {
    f[PLAYER_XP] = 0;
    f[PLAYER_NEXT_LEVEL_XP] = 0;
  }
  UpdateData update(_mapId);
  update.AddValuesUpdate(_playerGuid, f);
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  SendPacket(pkt);
  SendNotification("Level set to " + std::to_string(static_cast<int>(lv)) + ".");
  return true;
}

} // namespace Firelands
