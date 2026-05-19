#include <application/logic/GossipLogic.h>
#include <application/logic/QuestGiverLogic.h>
#include <domain/models/NpcText.h>
#include <domain/models/QuestGiverStatus.h>
#include <domain/repositories/IQuestGossipRepository.h>
#include <domain/repositories/INpcTextRepository.h>
#include <shared/network/packets/server/NpcTextPackets.h>
#include <shared/network/packets/server/QuestPackets.h>
#include <application/services/WorldService.h>
#include <domain/repositories/IGossipRepository.h>
#include <domain/repositories/INpcTemplateSearchRepository.h>
#include <domain/world/Creature.h>
#include <domain/world/Map.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/Logger.h>
#include <shared/game/UnitNpcFlags.h>
#include <shared/game/WowGuid.h>

namespace Firelands {

namespace ws_obj = WorldSessionObjectUpdate;

std::optional<uint32_t> WorldSession::TryResolveCreatureTemplateEntry(
    uint64_t npcGuid) const {
  if (npcGuid == 0)
    return std::nullopt;

  if (_mapId != 0) {
    if (auto map = WorldService::Instance().GetMap(_mapId)) {
      if (auto creature = map->TryGetCreature(npcGuid))
        return creature->GetEntry();
    }
  }

  uint32_t const fromGuid = ExtractCreatureEntryFromUnitObjectGuid(npcGuid);
  return fromGuid != 0 ? std::optional<uint32_t>{fromGuid} : std::nullopt;
}

bool WorldSession::TrySendDatabaseGossipMenu(uint64_t npcGuid,
                                             uint32_t templateEntry) {
  if (!_gossipRepo || templateEntry == 0)
    return false;

  uint32_t templateGossipMenuId = 0;
  uint32_t menuId = kDefaultNpcGossipMenuId;
  uint64_t npcFlags = 0;

  if (_npcTemplateSearch) {
    if (auto const row = _npcTemplateSearch->TryGetByEntry(templateEntry)) {
      templateGossipMenuId = row->gossipMenuId;
      menuId = ResolveGossipMenuIdForTemplate(templateGossipMenuId);
      npcFlags = row->npcFlags;
    }
  }

  if (!CreatureUsesGossipMenuDialog(templateGossipMenuId, npcFlags))
    return false;

  auto options = _gossipRepo->GetMenuOptions(menuId);
  options = FilterGossipOptionsByNpcFlags(std::move(options), npcFlags);

  auto const textId = _gossipRepo->GetMenuTextId(menuId);

  std::vector<GossipQuestItem> quests;
  if (_questGossipRepo) {
    auto summaries = _questGossipRepo->GetStarterQuestsForCreature(templateEntry);
    summaries = FilterQuestGossipForPlayer(std::move(summaries), _playerClass,
                                           _playerRace);
    quests = BuildGossipQuestItems(summaries);
  }

  if (!ShouldSendGossipMenu(options.size(), textId.has_value(), quests.size()))
    return false;

  uint32_t const wireTextId = textId.value_or(0);
  LOG_DEBUG("Gossip open entry={} menu={} textId={} options={} quests={}",
            templateEntry, menuId, wireTextId, options.size(), quests.size());
  SendGossipMessage(npcGuid, menuId, wireTextId, options, quests);
  return true;
}

void WorldSession::SendNpcTextForGossipWindow(uint32_t textId) {
  NpcText payload = NpcText::MakeFallback(textId);
  if (!TryBuildGmTicketNpcText(textId, payload)) {
    if (_npcTextRepo) {
      if (auto const loaded = _npcTextRepo->TryGetById(textId))
        payload = *loaded;
    }
  }
  EnsureNpcTextGreeting(payload);

  WorldPacket npcTextPkt = gossip::BuildNpcTextUpdate(payload);
  LOG_DEBUG("SMSG_NPC_TEXT_UPDATE textId={} payload={}", textId, npcTextPkt.Size());
  SendPacket(npcTextPkt);
}

bool WorldSession::TryOpenQuestGiverDialog(uint64_t npcGuid) {
  if (npcGuid == 0 || _playerGuid == 0)
    return false;

  if (auto const entry = TryResolveCreatureTemplateEntry(npcGuid)) {
    uint32_t templateGossipMenuId = 0;
    uint64_t npcFlags = 0;
    if (_npcTemplateSearch) {
      if (auto const row = _npcTemplateSearch->TryGetByEntry(*entry)) {
        templateGossipMenuId = row->gossipMenuId;
        npcFlags = row->npcFlags;
      }
    }

    if (CreatureUsesGossipMenuDialog(templateGossipMenuId, npcFlags)) {
      if (TrySendDatabaseGossipMenu(npcGuid, *entry))
        return true;
    }

    if (_questGossipRepo) {
      auto summaries = _questGossipRepo->GetStarterQuestsForCreature(*entry);
      summaries = FilterQuestGossipForPlayer(std::move(summaries), _playerClass,
                                             _playerRace);
      auto const quests = BuildGossipQuestItems(summaries);
      if (!quests.empty()) {
        LOG_DEBUG("Quest list open entry={} quests={}", *entry, quests.size());
        WorldPacket data =
            quest::BuildQuestGiverQuestListMessage(npcGuid, "Greetings $N", quests);
        SendPacket(data);
        return true;
      }
    }
  }
  return false;
}

uint32_t WorldSession::ResolveEffectiveNpcFlagsForCreature(
    Creature const &creature) const {
  return EffectiveUnitNpcFlagsForCreature(
      creature.GetNpcFlags(),
      CreatureHasStarterQuests(_questGossipRepo.get(), creature.GetEntry(),
                               _playerClass, _playerRace));
}

void WorldSession::SendQuestGiverStatusForGuid(uint64_t npcGuid,
                                               uint32_t creatureEntry) {
  if (npcGuid == 0 || creatureEntry == 0)
    return;
  auto const status = ResolveQuestGiverDialogStatus(
      _questGossipRepo.get(), creatureEntry, _playerClass, _playerRace);
  if (status == QuestGiverDialogStatus::None)
    return;
  auto data = quest::BuildQuestGiverStatus(npcGuid, static_cast<uint32_t>(status));
  SendPacket(data);
}

void WorldSession::SendQuestGiverStatusMultipleNearby() {
  if (!_questGossipRepo)
    return;
  auto map = WorldService::Instance().GetMap(_mapId);
  if (!map)
    return;

  std::vector<quest::QuestGiverStatusEntry> entries;
  map->ForEachCreatureNear(_position.x, _position.y, 2,
                           [&](std::shared_ptr<Creature> const &cr) {
                             auto const status = ResolveQuestGiverDialogStatus(
                                 _questGossipRepo.get(), cr->GetEntry(),
                                 _playerClass, _playerRace);
                             if (status == QuestGiverDialogStatus::None)
                               return;
                             entries.push_back(
                                 {cr->GetGuid(), static_cast<uint32_t>(status)});
                           });

  if (entries.empty())
    return;
  auto data = quest::BuildQuestGiverStatusMultiple(entries);
  SendPacket(data);
}

void WorldSession::HandleQuestGiverHello(WorldPacket &packet) {
  const uint64_t npcGuid = ws_obj::ReadClientQuestGiverGuid(packet);
  if (npcGuid == 0 || _playerGuid == 0)
    return;

  _gossipMenuSent = false;
  if (auto host = WorldService::Instance().GetScriptHost())
    host->FireGossipHello(npcGuid);

  if (!_gossipMenuSent && TryOpenQuestGiverDialog(npcGuid))
    return;

  SendGossipComplete();
}

void WorldSession::HandleQuestGiverStatusQuery(WorldPacket &packet) {
  const uint64_t npcGuid = ws_obj::ReadClientQuestGiverGuid(packet);
  if (npcGuid == 0)
    return;
  if (auto const entry = TryResolveCreatureTemplateEntry(npcGuid))
    SendQuestGiverStatusForGuid(npcGuid, *entry);
}

void WorldSession::HandleQuestGiverStatusMultipleQuery(WorldPacket &) {
  SendQuestGiverStatusMultipleNearby();
}

void WorldSession::HandleQuestGiverQueryQuest(WorldPacket &packet) {
  const uint64_t npcGuid = ws_obj::ReadClientQuestGiverGuid(packet);
  if (npcGuid == 0 || packet.GetReadPos() + sizeof(uint32_t) > packet.Size())
    return;
  const uint32_t questId = packet.Read<uint32_t>();
  (void)questId;
  // Quest accept/details not implemented yet; tell client the quest is unavailable.
  auto data = quest::BuildQuestGiverInvalidQuest();
  SendPacket(data);
}

void WorldSession::HandleTaxiNodeStatusQuery(WorldPacket &packet) {
  const uint64_t unitGuid = ws_obj::ReadClientQuestGiverGuid(packet);
  if (unitGuid == 0)
    return;
  // Only flight masters get SMSG_TAXI_NODE_STATUS; quest NPCs must not keep FLIGHTMASTER
  // in UNIT_NPC_FLAGS (see EffectiveUnitNpcFlagsForCreature).
  if (auto const entry = TryResolveCreatureTemplateEntry(unitGuid)) {
    uint32_t npcFlags = 0;
    if (_npcTemplateSearch) {
      if (auto const row = _npcTemplateSearch->TryGetByEntry(*entry))
        npcFlags = static_cast<uint32_t>(row->npcFlags);
    }
    if ((npcFlags & kUnitNpcFlagFlightMaster) == 0)
      return;
  }
  WorldPacket data(SMSG_TAXI_NODE_STATUS, 12);
  data.WritePackedGuid(unitGuid);
  data.Append<uint8_t>(2); // unknown node (no taxi DB wired)
  SendPacket(data);
}

} // namespace Firelands
