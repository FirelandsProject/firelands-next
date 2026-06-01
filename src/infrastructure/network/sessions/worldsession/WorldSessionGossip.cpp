#include <application/logic/GossipLogic.h>
#include <application/logic/QuestGiverLogic.h>
#include <application/logic/QuestProgressLogic.h>
#include <domain/models/QuestProgress.h>
#include <domain/models/NpcText.h>
#include <domain/models/QuestGiverStatus.h>
#include <domain/repositories/IQuestGossipRepository.h>
#include <domain/repositories/INpcTextRepository.h>
#include <shared/network/packets/server/NpcTextPackets.h>
#include <shared/network/BitReader.h>
#include <shared/network/packets/server/QuestPackets.h>
#include <application/services/WorldService.h>
#include <domain/repositories/IGossipRepository.h>
#include <domain/repositories/INpcTemplateSearchRepository.h>
#include <domain/world/Creature.h>
#include <domain/world/Map.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/Logger.h>
#include <shared/game/GmCreatureVisibility.h>
#include <shared/game/PlayerQuestLog.h>
#include <shared/game/UnitNpcFlags.h>
#include <shared/game/WowGuid.h>

namespace Firelands {

namespace ws_obj = WorldSessionObjectUpdate;

std::optional<uint32_t> WorldSession::TryResolveCreatureTemplateEntry(
    uint64_t npcGuid) const {
  if (npcGuid == 0)
    return std::nullopt;

  if (_mapId != 0) {
    if (auto map = runtime().GetMap(_mapId)) {
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
  if (_questGossipRepo)
    quests = BuildAllGossipQuestItemsForPlayer(
        _questGossipRepo.get(), templateEntry, _playerClass, _playerRace, _playerLevel,
        _questProgress);

  if (!ShouldSendGossipMenu(options.size(), textId.has_value(), quests.size()))
    return false;

  uint32_t const wireTextId = textId.value_or(0);
  LOG_DEBUG("Gossip open entry={} menu={} textId={} options={} quests={} opcode={:#x}",
            templateEntry, menuId, wireTextId, options.size(), quests.size(),
            static_cast<uint32_t>(SMSG_GOSSIP_MESSAGE));
  SendGossipMessage(npcGuid, menuId, wireTextId, options, quests);
  return true;
}

void WorldSession::SendNpcTextForGossipWindow(uint32_t textId) {
  NpcText payload = NpcText::MakeFallback(textId);
  if (!TryBuildGmTicketNpcText(textId, payload)) {
    if (!TryBuildGmNpcInfoNpcText(textId, payload)) {
      if (_npcTextRepo) {
        if (auto const loaded = _npcTextRepo->TryGetById(textId))
          payload = *loaded;
      }
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
    if (_questGossipRepo)
      TryProgressMeetQuestsAtCreature(*entry);

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
      auto const quests = BuildAllGossipQuestItemsForPlayer(
          _questGossipRepo.get(), *entry, _playerClass, _playerRace, _playerLevel,
          _questProgress);
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
                               _playerClass, _playerRace, _playerLevel,
                               _questProgress));
}

WorldSession::CreatureClientWireFields WorldSession::ResolveCreatureWireFieldsForClient(
    Creature const &creature) const {
  uint32 const effectiveNpc = ResolveEffectiveNpcFlagsForCreature(creature);
  bool const gmSeeAll = GmSeesAllCreatures();
  return {
      WireDisplayIdForCreature(creature.GetDisplayId(), creature.GetExtraFlags(), gmSeeAll),
      WireNpcFlagsForCreature(creature.GetNpcFlags(), effectiveNpc, gmSeeAll),
      WireUnitFieldFlagsForCreature(creature.GetUnitFieldFlags(), gmSeeAll),
      creature.GetUnitFieldFlags2(),
  };
}

void WorldSession::SendQuestGiverStatusForGuid(uint64_t npcGuid,
                                               uint32_t creatureEntry) {
  if (npcGuid == 0 || creatureEntry == 0)
    return;
  if (_questGossipRepo)
    TryProgressMeetQuestsAtCreature(creatureEntry);
  auto const status = ResolveQuestGiverDialogStatus(
      _questGossipRepo.get(), creatureEntry, _playerClass, _playerRace, _playerLevel,
      _questProgress);
  if (status == QuestGiverDialogStatus::None)
    return;
  auto data = quest::BuildQuestGiverStatus(npcGuid, static_cast<uint32_t>(status));
  SendPacket(data);
}

void WorldSession::SendQuestGiverStatusMultipleNearby() {
  if (!_questGossipRepo)
    return;
  auto map = runtime().GetMap(_mapId);
  if (!map)
    return;

  std::vector<quest::QuestGiverStatusEntry> entries;
  map->ForEachCreatureNear(_position.x, _position.y, 2,
                           [&](std::shared_ptr<Creature> const &cr) {
                             if (!IsCreatureVisibleToPlayer(*cr))
                               return;
                             auto const status = ResolveQuestGiverDialogStatus(
                                 _questGossipRepo.get(), cr->GetEntry(),
                                 _playerClass, _playerRace, _playerLevel,
                                 _questProgress);
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

void WorldSession::TryProgressMeetQuestsAtCreature(uint32_t creatureEntry) {
  if (!_questGossipRepo || creatureEntry == 0 || _playerGuid == 0)
    return;

  bool changed = false;
  for (QuestGossipSummary const &summary :
       _questGossipRepo->GetEnderQuestsForCreature(creatureEntry)) {
    if (!QuestGossipAllowsPlayer(summary, _playerClass, _playerRace))
      continue;
    if (_questProgress.GetQuestStatus(summary.questId) != QuestStatus::Incomplete)
      continue;
    if (!QuestCompletesOnMeetNpc(summary, creatureEntry))
      continue;

    _questProgress.SetQuestStatus(summary.questId, QuestStatus::Complete);
    MarkQuestProgressDirty();
    if (!SendPlayerQuestLogSlotWire(summary.questId))
      continue;
    WorldPacket updateComplete = quest::BuildQuestUpdateComplete(summary.questId);
    SendPacket(updateComplete);
    changed = true;
    LOG_DEBUG("Quest meet complete: id={} ender={}", summary.questId, creatureEntry);
  }

  if (!changed)
    return;
  PersistQuestProgressForCharacter();
  RefreshPlayerPhaseVisibilityFromQuestProgress();
}

void WorldSession::HandleQuestGiverHello(WorldPacket &packet) {
  const uint64_t npcGuid = ws_obj::ReadClientQuestGiverGuid(packet);
  if (npcGuid == 0 || _playerGuid == 0)
    return;

  if (auto const entry = TryResolveCreatureTemplateEntry(npcGuid))
    TryProgressMeetQuestsAtCreature(*entry);

  _gossipMenuSent = false;
  if (auto host = runtime().GetScriptHost())
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

void WorldSession::HandleQuestPoiQuery(WorldPacket &packet) {
  if (!_questGossipRepo || packet.GetReadPos() + sizeof(uint32_t) > packet.Size())
    return;
  uint32_t const count = packet.Read<uint32_t>();
  if (count == 0 || count > kMaxQuestLogSlots)
    return;

  std::vector<uint32_t> questIds;
  questIds.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    if (packet.GetReadPos() + sizeof(uint32_t) > packet.Size())
      break;
    uint32_t const questId = packet.Read<uint32_t>();
    if (questId == 0)
      continue;
    if (!_questGossipRepo || !_questGossipRepo->TryGetQuestTemplate(questId))
      continue;
    questIds.push_back(questId);
  }

  if (questIds.empty())
    return;

  WorldPacket poiResponse = quest::BuildQuestPoiQueryResponse(questIds);
  SendPacket(poiResponse);
}

void WorldSession::HandleQuestNpcQuery(WorldPacket &packet) {
  if (!_questGossipRepo || packet.Size() < 3)
    return;

  BitReader br(packet);
  uint32_t const count = br.ReadBits(24);
  if (count == 0 || count > kMaxQuestLogSlots)
    return;

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> quests;
  quests.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    if (packet.GetReadPos() + sizeof(uint32_t) > packet.Size())
      break;
    uint32_t const questId = packet.Read<uint32_t>();
    if (questId == 0 || !_questGossipRepo->TryGetQuestTemplate(questId))
      continue;
    quests.emplace_back(
        questId, _questGossipRepo->GetInvolvedCreatureEntriesForQuest(questId));
  }

  if (quests.empty())
    return;

  WorldPacket npcResponse = quest::BuildQuestNpcQueryResponse(quests);
  SendPacket(npcResponse);
}

void WorldSession::HandleQuestQuery(WorldPacket &packet) {
  if (packet.GetReadPos() + sizeof(uint32_t) > packet.Size())
    return;
  uint32_t const questId = packet.Read<uint32_t>();
  if (questId == 0 || !_questGossipRepo)
    return;

  auto const summary = _questGossipRepo->TryGetQuestTemplate(questId);
  if (!summary)
    return;

  WorldPacket response = quest::BuildQuestQueryResponse(*summary);
  SendPacket(response);
}

void WorldSession::HandleQuestLogRemoveQuest(WorldPacket &packet) {
  if (packet.GetReadPos() + sizeof(uint8_t) > packet.Size() || _playerGuid == 0)
    return;
  uint8_t const slot = packet.Read<uint8_t>();
  if (slot >= kMaxQuestLogSlots)
    return;

  uint32_t const questId = _questProgress.GetQuestLogSlotQuestId(slot);
  if (questId == 0)
    return;

  _questProgress.SetQuestStatus(questId, QuestStatus::None);
  MarkQuestProgressDirty();
  (void)ClearPlayerQuestLogSlotWire(slot);
  PersistQuestProgressForCharacter();
  SendQuestGiverStatusMultipleNearby();
}

void WorldSession::HandleQuestGiverQueryQuest(WorldPacket &packet) {
  const uint64_t npcGuid = ws_obj::ReadClientQuestGiverGuid(packet);
  if (npcGuid == 0 || packet.GetReadPos() + sizeof(uint32_t) > packet.Size())
    return;
  const uint32_t questId = packet.Read<uint32_t>();
  ws_obj::ReadQuestGiverClientTail(packet);

  if (!_questGossipRepo || questId == 0)
    return;

  auto const entry = TryResolveCreatureTemplateEntry(npcGuid);
  if (!entry)
    return;

  if (auto const ender =
          FindEnderQuestForCreature(_questGossipRepo.get(), *entry, questId)) {
    TryProgressMeetQuestsAtCreature(*entry);
    if (_questProgress.GetQuestStatus(questId) == QuestStatus::Complete) {
      if (TrySendQuestGiverOfferReward(npcGuid, *entry, *ender))
        return;
    }
    WorldPacket invalid = quest::BuildQuestGiverInvalidQuest();
    SendPacket(invalid);
    return;
  }

  auto const summary =
      FindStarterQuestForCreature(_questGossipRepo.get(), *entry, questId);
  if (!summary) {
    WorldPacket invalid = quest::BuildQuestGiverInvalidQuest();
    SendPacket(invalid);
    return;
  }

  QuestAcceptResult const acceptCheck =
      EvaluateQuestAccept(*summary, _questProgress, _playerClass, _playerRace,
                          _playerLevel);
  if (acceptCheck != QuestAcceptResult::Accepted) {
    WorldPacket invalid =
        quest::BuildQuestGiverInvalidQuest(QuestAcceptResultToInvalidReason(acceptCheck));
    SendPacket(invalid);
    return;
  }

  if (QuestHasAutoAcceptFlag(summary->flags)) {
    QuestGrantSideEffects preview{};
    preview.persist = false;
    preview.refreshPhase = false;
    preview.refreshNearbyQuestMarkers = false;
    (void)TryGrantQuestFromGiver(npcGuid, *entry, *summary, preview);
    if (_questProgress.GetQuestStatus(questId) != QuestStatus::None)
      (void)SendPlayerQuestLogSlotWire(questId);
  }

  WorldPacket details =
      quest::BuildQuestGiverQuestDetails(npcGuid, questId, *summary);
  SendPacket(details);
}

bool WorldSession::TrySendQuestGiverOfferReward(uint64_t npcGuid, uint32_t creatureEntry,
                                                QuestGossipSummary const &summary) {
  if (npcGuid == 0 || creatureEntry == 0 || summary.questId == 0)
    return false;
  if (_questProgress.GetQuestStatus(summary.questId) != QuestStatus::Complete)
    return false;

  WorldPacket offer = quest::BuildQuestGiverOfferReward(
      npcGuid, summary.questId, summary, QuestOfferRewardTextForPlayer(summary));
  SendPacket(offer);
  LOG_DEBUG("Quest offer reward: id={} creature={}", summary.questId, creatureEntry);
  return true;
}

void WorldSession::HandleQuestGiverRequestReward(WorldPacket &packet) {
  const uint64_t npcGuid = ws_obj::ReadClientQuestGiverGuid(packet);
  if (npcGuid == 0 || packet.GetReadPos() + sizeof(uint32_t) > packet.Size())
    return;
  const uint32_t questId = packet.Read<uint32_t>();

  if (!_questGossipRepo || questId == 0)
    return;

  auto const entry = TryResolveCreatureTemplateEntry(npcGuid);
  if (!entry)
    return;

  auto const summary =
      FindEnderQuestForCreature(_questGossipRepo.get(), *entry, questId);
  if (!summary) {
    WorldPacket invalid = quest::BuildQuestGiverInvalidQuest();
    SendPacket(invalid);
    return;
  }

  TryProgressMeetQuestsAtCreature(*entry);
  if (!TrySendQuestGiverOfferReward(npcGuid, *entry, *summary)) {
    WorldPacket invalid = quest::BuildQuestGiverInvalidQuest();
    SendPacket(invalid);
  }
}

bool WorldSession::TryRewardQuestFromEnder(uint64_t npcGuid, uint32_t creatureEntry,
                                           uint32_t questId) {
  if (!_questGossipRepo || questId == 0 || _playerGuid == 0)
    return false;

  auto const summary =
      FindEnderQuestForCreature(_questGossipRepo.get(), creatureEntry, questId);
  if (!summary)
    return false;

  if (_questProgress.GetQuestStatus(questId) != QuestStatus::Complete)
    return false;

  std::optional<uint8_t> const logSlot = _questProgress.FindQuestLogSlot(questId);
  _questProgress.SetQuestRewarded(questId);
  MarkQuestProgressDirty();
  if (logSlot)
    (void)ClearPlayerQuestLogSlotWire(*logSlot);
  PersistQuestProgressForCharacter();
  WorldPacket questComplete = quest::BuildQuestGiverQuestComplete(questId);
  SendPacket(questComplete);
  if (summary->soundTurnIn != 0) {
    WorldPacket sound = quest::BuildPlaySound(_playerGuid, summary->soundTurnIn);
    SendPacket(sound);
  }
  RefreshPlayerPhaseVisibilityFromQuestProgress();
  if (npcGuid != 0 && creatureEntry != 0)
    SendQuestGiverStatusForGuid(npcGuid, creatureEntry);
  SendGossipComplete();
  LOG_DEBUG("Quest rewarded: id={} creature={}", questId, creatureEntry);
  return true;
}

void WorldSession::HandleQuestGiverChooseReward(WorldPacket &packet) {
  const uint64_t npcGuid = ws_obj::ReadClientQuestGiverGuid(packet);
  if (npcGuid == 0 || packet.GetReadPos() + sizeof(uint32_t) > packet.Size())
    return;
  const uint32_t questId = packet.Read<uint32_t>();
  if (packet.GetReadPos() + sizeof(uint32_t) <= packet.Size())
    (void)packet.Read<uint32_t>(); // ItemChoiceID

  auto const entry = TryResolveCreatureTemplateEntry(npcGuid);
  if (!entry)
    return;

  if (!TryRewardQuestFromEnder(npcGuid, *entry, questId)) {
    WorldPacket invalid = quest::BuildQuestGiverInvalidQuest();
    SendPacket(invalid);
    SendGossipComplete();
  }
}

bool WorldSession::ClearPlayerQuestLogSlotWire(uint8_t slot) {
  if (_playerGuid == 0 || slot >= kMaxQuestLogSlots)
    return false;
  WorldPacket logPkt;
  ws_obj::BuildPlayerQuestLogSlotValuesUpdate(static_cast<uint16>(_mapId), _playerGuid,
                                              slot, 0u, 0u, 0u, logPkt);
  if (logPkt.Size() == 0)
    return false;
  SendPacket(logPkt);
  return true;
}

bool WorldSession::SendPlayerQuestLogSlotWire(uint32_t questId) {
  if (_playerGuid == 0 || questId == 0)
    return false;
  auto const slot = _questProgress.FindQuestLogSlot(questId);
  if (!slot)
    return false;
  uint32_t const stateFlags =
      _questProgress.GetQuestStatus(questId) == QuestStatus::Complete
          ? kPlayerQuestLogStateComplete
          : 0u;
  WorldPacket logPkt;
  ws_obj::BuildPlayerQuestLogSlotValuesUpdate(
      static_cast<uint16>(_mapId), _playerGuid, *slot, questId, stateFlags, 0u,
      logPkt);
  if (logPkt.Size() == 0)
    return false;
  SendPacket(logPkt);
  return true;
}

bool WorldSession::TryGrantQuestFromGiver(uint64_t npcGuid, uint32_t creatureEntry,
                                          QuestGossipSummary const &summary,
                                          QuestGrantSideEffects sideEffects) {
  uint32_t const questId = summary.questId;
  if (questId == 0 || _playerGuid == 0)
    return false;

  if (EvaluateQuestAccept(summary, _questProgress, _playerClass, _playerRace,
                          _playerLevel) != QuestAcceptResult::Accepted)
    return false;

  if (!_questProgress.HasFreeQuestLogSlot() &&
      !_questProgress.FindQuestLogSlot(questId).has_value())
    return false;

  if (QuestHasAutoCompleteFlag(summary.flags)) {
    _questProgress.SetQuestStatus(questId, QuestStatus::Complete);
    if (!SendPlayerQuestLogSlotWire(questId))
      return false;
    WorldPacket updateComplete = quest::BuildQuestUpdateComplete(questId);
    SendPacket(updateComplete);
    std::optional<uint8_t> const logSlot = _questProgress.FindQuestLogSlot(questId);
    _questProgress.SetQuestRewarded(questId);
    if (logSlot)
      (void)ClearPlayerQuestLogSlotWire(*logSlot);
    WorldPacket questComplete = quest::BuildQuestGiverQuestComplete(questId);
    SendPacket(questComplete);
  } else {
    _questProgress.SetQuestStatus(questId, QuestStatus::Incomplete);
    if (!SendPlayerQuestLogSlotWire(questId)) {
      _questProgress.SetQuestStatus(questId, QuestStatus::None);
      return false;
    }
  }

  if (summary.soundAccept != 0) {
    WorldPacket sound = quest::BuildPlaySound(_playerGuid, summary.soundAccept);
    SendPacket(sound);
  }

  MarkQuestProgressDirty();
  LOG_DEBUG("Quest granted: id={} creature={} slot={}", questId, creatureEntry,
            _questProgress.FindQuestLogSlot(questId).value_or(255));
  if (sideEffects.persist)
    PersistQuestProgressForCharacter();
  if (sideEffects.refreshPhase)
    RefreshPlayerPhaseVisibilityFromQuestProgress();
  if (sideEffects.refreshNearbyQuestMarkers)
    SendQuestGiverStatusMultipleNearby();
  else if (npcGuid != 0 && creatureEntry != 0)
    SendQuestGiverStatusForGuid(npcGuid, creatureEntry);
  return true;
}

void WorldSession::HandleQuestGiverAcceptQuest(WorldPacket &packet) {
  const uint64_t npcGuid = ws_obj::ReadClientQuestGiverGuid(packet);
  if (npcGuid == 0 || packet.GetReadPos() + sizeof(uint32_t) > packet.Size())
    return;
  const uint32_t questId = packet.Read<uint32_t>();
  ws_obj::ReadQuestGiverClientTail(packet);

  if (!_questGossipRepo || questId == 0 || _playerGuid == 0) {
    LOG_WARN("Quest accept ignored: repo={} questId={} playerGuid={}",
             static_cast<bool>(_questGossipRepo), questId, _playerGuid);
    return;
  }

  auto const entry = TryResolveCreatureTemplateEntry(npcGuid);
  if (!entry) {
    LOG_WARN("Quest accept: no creature entry for guid={:#x} quest={}", npcGuid,
             questId);
    return;
  }

  auto const summary =
      FindStarterQuestForCreature(_questGossipRepo.get(), *entry, questId);
  if (!summary) {
    LOG_WARN("Quest accept: quest {} not on creature entry {}", questId, *entry);
    WorldPacket invalid = quest::BuildQuestGiverInvalidQuest();
    SendPacket(invalid);
    SendGossipComplete();
    return;
  }

  QuestAcceptResult const acceptResult =
      EvaluateQuestAccept(*summary, _questProgress, _playerClass, _playerRace,
                          _playerLevel);
  if (acceptResult == QuestAcceptResult::AlreadyOnQuest &&
      _questProgress.GetQuestStatus(questId) != QuestStatus::None) {
    SendPlayerQuestLogSlotWire(questId);
    PersistQuestProgressForCharacter();
    SendQuestGiverStatusForGuid(npcGuid, *entry);
    SendGossipComplete();
    return;
  }
  if (acceptResult != QuestAcceptResult::Accepted) {
    WorldPacket invalid =
        quest::BuildQuestGiverInvalidQuest(QuestAcceptResultToInvalidReason(acceptResult));
    SendPacket(invalid);
    SendGossipComplete();
    return;
  }

  QuestGrantSideEffects acceptEffects{};
  acceptEffects.persist = true;
  acceptEffects.refreshPhase = true;
  acceptEffects.refreshNearbyQuestMarkers = false;
  if (!TryGrantQuestFromGiver(npcGuid, *entry, *summary, acceptEffects)) {
    WorldPacket logFull = quest::BuildQuestLogFull();
    SendPacket(logFull);
    SendGossipComplete();
    return;
  }

  SendGossipComplete();
}

void WorldSession::HandleQuestGiverCompleteQuest(WorldPacket &packet) {
  const uint64_t npcGuid = ws_obj::ReadClientQuestGiverGuid(packet);
  if (npcGuid == 0 || packet.GetReadPos() + sizeof(uint32_t) > packet.Size())
    return;
  const uint32_t questId = packet.Read<uint32_t>();
  if (packet.GetReadPos() < packet.Size())
    (void)packet.Read<uint8_t>(); // FromScript

  auto const entry = TryResolveCreatureTemplateEntry(npcGuid);
  if (!entry)
    return;

  if (!TryRewardQuestFromEnder(npcGuid, *entry, questId)) {
    WorldPacket invalid = quest::BuildQuestGiverInvalidQuest();
    SendPacket(invalid);
    SendGossipComplete();
  }
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
