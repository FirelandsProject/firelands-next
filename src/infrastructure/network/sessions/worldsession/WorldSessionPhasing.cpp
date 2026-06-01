#include <application/world/PhaseAreaCatalog.h>
#include <application/world/PhaseGroupCatalog.h>
#include <application/world/PlayerPhaseShift.h>
#include <application/world/PlayerQuestProgressStore.h>
#include <shared/game/PlayerQuestLog.h>
#include <shared/dbc/AreaTableDbc.h>
#include <domain/world/Creature.h>
#include <domain/world/Map.h>
#include <domain/world/Player.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/game/GmCreatureVisibility.h>
#include <shared/network/PhaseShiftWire.h>
#include <shared/network/UpdateData.h>
#include <shared/network/UpdateFields.h>
#include <functional>

namespace Firelands {

namespace ws_obj = WorldSessionObjectUpdate;

namespace {

constexpr uint32_t kMaxObjectsPerUpdatePacket = 48u;

void SendOutOfRangeObjectsInChunks(uint16 mapId,
                                   std::vector<uint64> const &guids,
                                   std::function<void(WorldPacket &)> const &send) {
  if (guids.empty())
    return;

  for (size_t i = 0; i < guids.size(); i += kMaxObjectsPerUpdatePacket) {
    size_t const end = std::min(i + kMaxObjectsPerUpdatePacket, guids.size());
    std::vector<uint64> chunk(guids.begin() + static_cast<std::ptrdiff_t>(i),
                              guids.begin() + static_cast<std::ptrdiff_t>(end));
    UpdateData batch(mapId);
    batch.AddOutOfRangeObjects(chunk);
    WorldPacket pkt;
    batch.Build(pkt);
    send(pkt);
  }
}

} // namespace

void WorldSession::RefreshPlayerPhaseVisibilityFromQuestProgress() {
  RefreshPlayerPhaseVisibilityFromAuras();
}

void WorldSession::LoadQuestProgressForCharacter(uint32 characterGuid) {
  if (!_questProgressRepo || characterGuid == 0)
    return;
  _questProgress.LoadSnapshot(_questProgressRepo->LoadForCharacter(characterGuid));
  _questProgressDirty = false;
}

void WorldSession::MarkQuestProgressDirty() { _questProgressDirty = true; }

void WorldSession::PersistQuestProgressForCharacter(bool force) {
  if (!_questProgressRepo || _playerGuid == 0)
    return;
  if (!force && !_questProgressDirty)
    return;
  uint32_t const characterGuid = static_cast<uint32_t>(_playerGuid);
  if (!_questProgressRepo->SaveForCharacter(characterGuid,
                                            _questProgress.ExportSnapshot())) {
    LOG_WARN("Quest progress save failed for guid {}", characterGuid);
    return;
  }
  _questProgressDirty = false;
}

void WorldSession::SendRestoredQuestLogToClient() {
  for (uint8_t slot = 0; slot < kMaxQuestLogSlots; ++slot) {
    uint32_t const questId = _questProgress.GetQuestLogSlotQuestId(slot);
    if (questId != 0)
      (void)SendPlayerQuestLogSlotWire(questId);
  }
}

void WorldSession::RefreshPlayerPhaseVisibilityFromAuras() {
  RebuildPlayerPhaseShiftFromActiveAuras();
  SendPlayerPhaseShiftToClient();
  RefreshNearbyCreaturePhaseVisibility(_position.x, _position.y);
}

bool WorldSession::IsCreatureVisibleToPlayer(Creature const &creature) const {
  return CreatureVisibleToViewer(_playerPhaseShift, creature.GetPhaseShift(),
                                 GmSeesAllCreatures());
}

void WorldSession::RebuildPlayerPhaseShiftFromActiveAuras() {
  auto map = runtime().GetMap(_mapId);
  if (!map || _playerGuid == 0)
    return;
  auto player = map->TryGetPlayer(_playerGuid);
  if (!player)
    return;

  std::function<std::vector<uint16>(uint32)> const resolveGroup =
      [this](uint32 groupId) -> std::vector<uint16> {
        if (auto catalog = runtime().GetPhaseGroupCatalog())
          return catalog->Resolve(groupId);
        return {};
      };

  std::vector<uint16> areaPhases;
  if (auto areaCatalog = runtime().GetPhaseAreaCatalog()) {
    std::function<uint32(uint32)> parentOf;
    if (auto table = runtime().GetAreaTableDbc()) {
      if (table->IsLoaded()) {
        parentOf = [table](uint32 area) { return table->GetParentAreaId(area); };
      }
    }
    _questProgress.SetAuraChecker([&player](uint32 spellId) {
      for (Aura const &aura : player->GetActiveAuras()) {
        if (aura.GetSpellId() == spellId)
          return true;
      }
      return false;
    });
    areaPhases = areaCatalog->ResolveForArea(_areaId, _questProgress, parentOf);
  }

  _playerPhaseShift = BuildPlayerPhaseShift(
      areaPhases, player->GetActiveAuras(),
      runtime().GetSpellDefinitions().get(), resolveGroup);
  player->SetPhaseShift(_playerPhaseShift);
}

void WorldSession::SendPlayerPhaseShiftToClient() {
  if (_playerGuid == 0)
    return;
  WorldPacket phasePkt = PhaseShiftWire::BuildPhaseShiftChange(_playerGuid, _playerPhaseShift);
  SendPacket(phasePkt);
}

void WorldSession::RefreshNearbyCreaturePhaseVisibility(float x, float y) {
  auto map = runtime().GetMap(_mapId);
  if (!map)
    return;

  std::unordered_set<uint64> nowVisible;
  map->ForEachCreatureNear(x, y, 1, [&](std::shared_ptr<Creature> const &cr) {
    if (IsCreatureVisibleToPlayer(*cr))
      nowVisible.insert(cr->GetGuid());
  });

  std::vector<uint64> toCreate;
  std::vector<uint64> toRemove;
  toCreate.reserve(nowVisible.size());
  for (uint64 const guid : nowVisible) {
    if (_visibleCreatureGuids.find(guid) == _visibleCreatureGuids.end())
      toCreate.push_back(guid);
  }
  for (uint64 const guid : _visibleCreatureGuids) {
    if (nowVisible.find(guid) == nowVisible.end())
      toRemove.push_back(guid);
  }

  if (!toCreate.empty()) {
    uint16 const mapIdU16 = static_cast<uint16>(_mapId);
    UpdateData batch(mapIdU16);
    uint32_t inBatch = 0;
    auto flush = [this, mapIdU16, &batch, &inBatch]() {
      if (batch.GetBlockCount() == 0)
        return;
      WorldPacket pkt;
      batch.Build(pkt);
      SendPacket(pkt);
      batch = UpdateData(mapIdU16);
      inBatch = 0;
    };

    for (uint64 const guid : toCreate) {
      auto cr = map->TryGetCreature(guid);
      if (!cr)
        continue;
      auto const wire = ResolveCreatureWireFieldsForClient(*cr);
      auto fields = ws_obj::BuildMinimalNpcUnitCreateFields(
          cr->GetGuid(), cr->GetEntry(), wire.displayId, cr->GetLiveHealth(),
          cr->GetLiveMaxHealth(), cr->GetLevel(), wire.npcFlags, cr->GetFactionTemplate(),
          wire.unitFieldFlags, wire.unitFieldFlags2, cr->GetUnitDynamicFlags(),
          &cr->GetCombatStats());
      batch.AddCreateObject(cr->GetGuid(), TYPEID_UNIT, cr->GetPosition(), fields);
      ++inBatch;
      if (inBatch >= kMaxObjectsPerUpdatePacket)
        flush();
    }
    flush();
  }

  if (!toRemove.empty()) {
    SendOutOfRangeObjectsInChunks(
        static_cast<uint16>(_mapId), toRemove,
        [this](WorldPacket &pkt) { SendPacket(pkt); });
  }

  _visibleCreatureGuids = std::move(nowVisible);
}

void WorldSession::RefreshNearbyCreatureGmWireFlags() {
  if (_playerGuid == 0 || _visibleCreatureGuids.empty())
    return;
  auto map = runtime().GetMap(_mapId);
  if (!map)
    return;

  uint16 const mapIdU16 = static_cast<uint16>(_mapId);
  UpdateData batch(mapIdU16);
  uint32_t inBatch = 0;
  auto flush = [this, mapIdU16, &batch, &inBatch]() {
    if (batch.GetBlockCount() == 0)
      return;
    WorldPacket pkt;
    batch.Build(pkt);
    SendPacket(pkt);
    batch = UpdateData(mapIdU16);
    inBatch = 0;
  };

  for (uint64 const guid : _visibleCreatureGuids) {
    auto cr = map->TryGetCreature(guid);
    if (!cr)
      continue;
    auto const wire = ResolveCreatureWireFieldsForClient(*cr);
    std::map<uint16, uint32> patch;
    patch[static_cast<uint16>(UNIT_FIELD_DISPLAYID)] = wire.displayId;
    patch[static_cast<uint16>(UNIT_FIELD_NATIVEDISPLAYID)] = wire.displayId;
    patch[static_cast<uint16>(UNIT_FIELD_FLAGS)] = wire.unitFieldFlags;
    patch[static_cast<uint16>(UNIT_NPC_FLAGS)] = wire.npcFlags;
    batch.AddValuesUpdate(guid, patch);
    ++inBatch;
    if (inBatch >= kMaxObjectsPerUpdatePacket)
      flush();
  }
  flush();
}

} // namespace Firelands
