#include <application/services/WorldService.h>
#include <application/world/PhaseAreaCatalog.h>
#include <application/world/PhaseGroupCatalog.h>
#include <application/world/PlayerPhaseShift.h>
#include <domain/world/Creature.h>
#include <domain/world/Map.h>
#include <domain/world/Player.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/network/PhaseShiftWire.h>
#include <shared/network/UpdateData.h>

namespace Firelands {

namespace ws_obj = WorldSessionObjectUpdate;

void WorldSession::RefreshPlayerPhaseVisibilityFromAuras() {
  RebuildPlayerPhaseShiftFromActiveAuras();
  SendPlayerPhaseShiftToClient();
  RefreshNearbyCreaturePhaseVisibility(_position.x, _position.y);
}

bool WorldSession::IsCreatureVisibleToPlayer(Creature const &creature) const {
  return _playerPhaseShift.CanSee(creature.GetPhaseShift());
}

void WorldSession::RebuildPlayerPhaseShiftFromActiveAuras() {
  auto map = WorldService::Instance().GetMap(_mapId);
  if (!map || _playerGuid == 0)
    return;
  auto player = map->TryGetPlayer(_playerGuid);
  if (!player)
    return;

  auto const resolveGroup = [](uint32 groupId) -> std::vector<uint16> {
    if (auto catalog = WorldService::Instance().GetPhaseGroupCatalog())
      return catalog->Resolve(groupId);
    return {};
  };

  std::vector<uint16> areaPhases;
  if (auto areaCatalog = WorldService::Instance().GetPhaseAreaCatalog())
    areaPhases = areaCatalog->ResolveForArea(_zoneId);

  _playerPhaseShift = BuildPlayerPhaseShift(
      areaPhases, player->GetActiveAuras(),
      WorldService::Instance().GetSpellDefinitions().get(), resolveGroup);
  player->SetPhaseShift(_playerPhaseShift);
}

void WorldSession::SendPlayerPhaseShiftToClient() {
  if (_playerGuid == 0)
    return;
  WorldPacket phasePkt = PhaseShiftWire::BuildPhaseShiftChange(_playerGuid, _playerPhaseShift);
  SendPacket(phasePkt);
}

void WorldSession::RefreshNearbyCreaturePhaseVisibility(float x, float y) {
  auto map = WorldService::Instance().GetMap(_mapId);
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
    constexpr uint32_t kMaxPerPacket = 48;

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
      uint32 const npcFlags = ResolveEffectiveNpcFlagsForCreature(*cr);
      auto fields = ws_obj::BuildMinimalNpcUnitCreateFields(
          cr->GetGuid(), cr->GetEntry(), cr->GetDisplayId(), cr->GetLiveHealth(),
          cr->GetLiveMaxHealth(), cr->GetLevel(), npcFlags, cr->GetFactionTemplate());
      batch.AddCreateObject(cr->GetGuid(), TYPEID_UNIT, cr->GetPosition(), fields);
      ++inBatch;
      if (inBatch >= kMaxPerPacket)
        flush();
    }
    flush();
  }

  if (!toRemove.empty()) {
    UpdateData outOfRange(static_cast<uint16>(_mapId));
    outOfRange.AddOutOfRangeObjects(toRemove);
    WorldPacket pkt;
    outOfRange.Build(pkt);
    SendPacket(pkt);
  }

  _visibleCreatureGuids = std::move(nowVisible);
}

} // namespace Firelands
