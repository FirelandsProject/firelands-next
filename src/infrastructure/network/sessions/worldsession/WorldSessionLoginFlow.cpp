#include <application/combat/PlayerCombatStats.h>
#include <domain/ports/IMapNotifier.h>
#include <shared/game/PlayerClass.h>
#include <shared/game/PlayerPowerType.h>
#include <application/services/OnlineCharacterSessionRegistry.h>
#include <application/spell/PassiveSpellAuras.h>
#include <shared/game/StatFormulas.h>
#include <application/services/PlayerSpellbook.h>
#include <application/ports/IGameScriptHost.h>
#include <application/world/WorldRuntimeAccess.h>
#include <infrastructure/persistence/MySqlAccountDataRepository.h>
#include <domain/models/Character.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <domain/world/Creature.h>
#include <domain/world/Player.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionMovementChecks.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionSpellEffects.h>
#include <shared/Logger.h>
#include <shared/game/ChatLanguages.h>
#include <shared/game/PlayerFactionTeam.h>
#include <shared/game/PlayerGmAppearance.h>
#include <shared/game/PlayerItemProficiency.h>
#include <shared/game/StarterOpeningCinematic.h>
#include <shared/game/SkillLineCategories.h>
#include <shared/game/StarterSpellFilters.h>
#include <shared/game/WowGuid.h>
#include <shared/network/MovementStateQueries.h>
#include <shared/network/packets/client/PackedPlayerGuidWire.h>
#include <shared/network/UpdateData.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Firelands {

namespace {

namespace ws_obj = WorldSessionObjectUpdate;

std::optional<std::pair<uint32, uint32>> TryLivePlayerHealth(uint32 mapId,
                                                             uint64 guid) {
  if (guid == 0ull)
    return std::nullopt;
  auto map = WorldRuntime().GetMap(mapId);
  if (!map)
    return std::nullopt;
  auto pl = map->TryGetPlayer(guid);
  if (!pl)
    return std::nullopt;
  return std::make_pair(pl->GetLiveHealth(), pl->GetLiveMaxHealth());
}

std::optional<std::pair<uint32, uint32>> TryLivePlayerPower1(uint32 mapId,
                                                             uint64 guid) {
  if (guid == 0ull)
    return std::nullopt;
  auto map = WorldRuntime().GetMap(mapId);
  if (!map)
    return std::nullopt;
  auto pl = map->TryGetPlayer(guid);
  if (!pl)
    return std::nullopt;
  return std::make_pair(pl->GetLivePower1(), pl->GetLiveMaxPower1());
}

void RebuildKnownSpellIdSet(std::vector<uint32> const &ordered,
                            std::unordered_set<uint32> &outIds) {
  outIds.clear();
  outIds.reserve(ordered.size());
  for (uint32 sid : ordered)
    outIds.insert(sid);
}

} // namespace

void WorldSession::HandlePlayerLogin(WorldPacket &packet) {
  uint64 guid = 0;
  WorldPackets::Client::ReadLoginPackedPlayerGuid(packet, guid);

  _playerGuid = guid;
  _timeSyncNextCounter = 0;

  auto characterOpt = _charService->GetCharacterByGuid(guid);
  if (!characterOpt) {
    LOG_ERROR("PlayerLogin failed: Account={} GUID={} Reason=NotFound", _accountId, guid);
    Close();
    return;
}
  Character const &character = *characterOpt;
  _actionBarToggles = character.GetActionBarToggles();
  _activeActionBarSpec = 0;
  _sentOpeningCinematic = false;
  _tutorialInts = character.GetTutorialMask();
  _playerRace = character.GetRace();
  _playerClass = ToClassId(character.GetClass());
  _playerLevel = std::max<uint8>(1, character.GetLevel());
  _loginPower1 = character.GetPower1();
  _loginMaxPower1 = character.GetMaxPower1();
  _moneyCopper = character.GetMoney();
  _playerXp = character.GetXp();
  _playerRestBonus = character.GetRestBonus();
  _playerFacialHair = character.GetFacialHair();

  LOG_DEBUG("PlayerLogin: Account={} GUID={} Name='{}' Level={} Map={}",
            _accountId, guid, character.GetName(), character.GetLevel(),
            character.GetMapId());

  LoginSendAccountDataAndPreMapPackets(guid, character);
  LoginBuildKnownSpellsAndSendSpellbook(character);
  LoginSendMotdAndMetaPackets();

  MovementInfo move{};
  LoginResolveMapPosition(guid, character, move);

  LoginSpawnInWorld(guid, character, move);
  LoadQuestProgressForCharacter(static_cast<uint32>(guid));
  LoginSendCreateUpdatesAndMutualVisibility(guid, character, move);
  LoginFinalizeWorldEntry(guid);

  LOG_INFO("Player entered world: Account={} GUID={} Name='{}' Map={} Pos=({},{:.2},{:.2})",
           _accountId, guid, character.GetName(), character.GetMapId(),
           character.GetX(), character.GetY(), character.GetZ());
}

void WorldSession::LoginSendAccountDataAndPreMapPackets(
    uint64 guid, Character const &character) {
  // Login SMSG order aligned with Cataclysm 4.3.4 reference and Player::SendInitialPacketsBeforeAddToMap.
  SendDungeonDifficulty(false);

  _activeCharacterGuid = character.GetGuid();
  if (_accountDataRepo && _preLoginPerCharAccountDirtyMask != 0) {
    for (uint32_t t = 0; t < NUM_ACCOUNT_DATA_TYPES; ++t) {
      if (!IsPerCharacterAccountDataType(t))
        continue;
      if ((_preLoginPerCharAccountDirtyMask & (1u << t)) == 0)
        continue;
      if (_accountData[t].data.empty() && _accountData[t].time == 0)
        _accountDataRepo->DeleteCharacter(_activeCharacterGuid,
                                           static_cast<uint8_t>(t));
      else
        _accountDataRepo->UpsertCharacter(
            _activeCharacterGuid, static_cast<uint8_t>(t), _accountData[t].time,
            _accountData[t].data);
}
    _preLoginPerCharAccountDirtyMask = 0;
}
  ReloadCharacterAccountDataFromDb(_activeCharacterGuid);
  SendAccountDataTimes(kPerCharacterAccountDataMask);
  SendLearnedDanceMoves();
  SendHotfixNotifyBlobEmpty();

  // Player::SendInitialPacketsBeforeAddToMap (same relative order as reference)
  SendClientControlUpdate(guid);
  SendBindPointUpdate();
  SendWorldServerInfo();
}

void WorldSession::LoginBuildKnownSpellsAndSendSpellbook(Character const &character) {
  {
    PlayerCreateInfoService const *pci = _charService->GetPlayerCreateInfoService();
    if (pci) {
      _knownSpells = PlayerSpellbook::BuildKnownSpells(
          character.GetRace(), ToClassId(character.GetClass()), character.GetLevel(), *pci,
          _spellDefinitions.get(),
          _charService->GetCharacterSpellIds(character.GetGuid()));
      _knownSkills = PlayerSpellbook::BuildStarterSkills(
          character.GetRace(), ToClassId(character.GetClass()), *pci);
    } else {
      _knownSpells.clear();
      AppendRacialLanguageSpells(character.GetRace(), _knownSpells);
      _knownSkills.clear();
}
    if (character.GetClass() == PlayerClass::Warlock) {
      _knownSpells.erase(
          std::remove_if(_knownSpells.begin(), _knownSpells.end(),
                         [](uint32_t sid) {
                           return IsWarlockQuestGatedSummonSpell(sid);
                         }),
          _knownSpells.end());
}
    RebuildKnownSpellIdSet(_knownSpells, _knownSpellIds);

    for (uint32_t const persisted :
         _charService->GetCharacterSpellIds(character.GetGuid())) {
      if (_knownSpellIds.count(persisted) != 0)
        continue;
      bool const strip = [&] {
        if (IsGuildPerkSpell(persisted) ||
            IsWarlockQuestGatedSummonSpell(persisted) ||
            IsRidingOrTransportStarterSpell(persisted) ||
            IsKnownMountSpell(persisted))
          return true;
        if (IsSpellFromExcludedSkillLine(persisted))
          return true;
        if (!_spellDefinitions)
          return false;
        auto def = _spellDefinitions->GetDefinition(persisted);
        if (!def)
          return false;
        return def->hasMountOrVehicleAura || def->grantsSkillLine;
      }();
      if (strip)
        _charService->RemoveCharacterSpell(character.GetGuid(), persisted);
}

    SendSetProficiency(kItemClassWeapon,
                       ComputeWeaponProficiencyMask(_knownSkills));
    SendSetProficiency(kItemClassArmor,
                       ComputeArmorProficiencyMask(_knownSkills));

    uint32 const defaultLang = PrimaryLanguageForRace(character.GetRace());
    uint32 const defaultLangSpell = LanguageSpellIdForLang(defaultLang);
  {
      std::string ids;
      for (size_t i = 0; i < _knownSpells.size(); ++i) {
        if (i)
          ids.push_back(',');
        ids += std::to_string(_knownSpells[i]);
}
      LOG_DEBUG("[CHAT] login race={} class={} defaultLang={} langSpell={} known={} "
                "spells={} ids=[{}]",
                static_cast<uint32>(character.GetRace()),
                static_cast<uint32>(ToClassId(character.GetClass())), defaultLang,
                defaultLangSpell,
                PlayerKnowsLanguage(_knownSpellIds, defaultLang, character.GetRace()) ? 1 : 0,
                _knownSpells.size(), ids);
}
}
  _gcdReady = {};
  _gcdTriggerSpellId = 0;
  _spellCooldownUntil.clear();
  _spellCategoryCooldownUntil.clear();
  RestorePersistedSpellCooldowns(static_cast<uint32>(character.GetGuid()));
  // At world login this packet must initialize client spellbook state,
  // including passive language spells. Existing characters may have
  // `firstLogin = false`, but the client still expects InitialLogin=1 here.
  SendKnownSpells(true, _knownSpells);
  // Ref Player::SendUnlearnSpells: superseded-rank cleanup for spells the
  // player already has — not a list of quest-gated trainable ids. Sending 688 here
  // made Summon Imp show as learnable (yellow) on the client.
  SendUnlearnSpellsEmpty();
  SendTalentsInfo();
  LoadActionButtonsForCharacter(static_cast<uint32_t>(character.GetGuid()));
  SendInitialActionButtons();
  SendInitialFactions();
  SendContactListEmpty();
  SendForcedReactions();
  SendSetupCurrency();
  SendAllAchievementDataEmpty();
  SendLoginSetTimeSpeed();
  SendEquipmentSetListEmpty();
}

void WorldSession::RefreshKnownSpellsForCharacter(Character const &character) {
    PlayerCreateInfoService const *pci = _charService->GetPlayerCreateInfoService();
  if (!pci)
    return;
      _knownSpells = PlayerSpellbook::BuildKnownSpells(
          character.GetRace(), ToClassId(character.GetClass()), character.GetLevel(), *pci,
          _spellDefinitions.get(),
          _charService->GetCharacterSpellIds(character.GetGuid()));
    RebuildKnownSpellIdSet(_knownSpells, _knownSpellIds);
  SendKnownSpells(true, _knownSpells);
}

void WorldSession::LoginSendMotdAndMetaPackets() {
  // Reference sends MOTD and SMSG_FEATURE_SYSTEM_STATUS after BeforeAddToMap.
  SendMotd();
  SendFeatureSystemStatus();
  SendTutorialMask(_tutorialInts);
  SendClientCacheVersion(0);
}

void WorldSession::LoginResolveMapPosition(uint64 guid, Character const &character,
                                           MovementInfo &outMove) {
  // Create in-memory player and add to map BEFORE sending verify-world + worldstates
  _mapId = character.GetMapId();
  _zoneId = character.GetZoneId();
  _areaId = ResolveSessionAreaId(_zoneId);
  static auto startTime = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  outMove.time = static_cast<uint32>(
      std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime)
          .count());
  outMove.x = character.GetX();
  outMove.y = character.GetY();
  outMove.z = character.GetZ();
  outMove.orientation = character.GetOrientation();
  if (!WsIsSaneWorldPosition(outMove)) {
    // Recover from previously persisted corrupt movement values by snapping to
    // race starter position. This avoids endless loading screens on relog.
    if (auto fallback = FallbackStartPosition(character.GetRace())) {
      _mapId = fallback->mapId;
      _zoneId = static_cast<uint16>(std::min<uint32_t>(fallback->zoneId, 0xFFFFu));
      _areaId = ResolveSessionAreaId(_zoneId);
      outMove.x = fallback->x;
      outMove.y = fallback->y;
      outMove.z = fallback->z;
      outMove.orientation = fallback->orientation;
      LOG_WARN("Invalid saved position for guid {} (x={} y={} z={} o={}), "
               "using race fallback map={} zone={} x={} y={} z={} o={}",
               guid, character.GetX(), character.GetY(), character.GetZ(),
               character.GetOrientation(), _mapId, _zoneId, outMove.x, outMove.y,
               outMove.z, outMove.orientation);
    } else {
      LOG_WARN("Invalid saved position for guid {} and no race fallback; "
               "keeping DB values.", guid);
}
}
  // Seed session position immediately on login. If the user logs out before the
  // first movement heartbeat arrives, logout persistence must still save a valid
  // location instead of default zeros.
  _position = outMove;
}

void WorldSession::LoginSpawnInWorld(uint64 guid, Character const &character,
                                     MovementInfo const &move) {
  auto player = std::make_shared<Player>(guid, shared_from_this());
  player->SetPosition(move);
  player->SetRaceAndFaction(character.GetRace(), character.GetFactionTemplate());
  player->InitCombatResources(character.GetHealth(), character.GetMaxHealth(),
                               character.GetPower1(), character.GetMaxPower1());
  player->SetBaselineCombatStats(BuildPlayerCombatStats(character));
  {
    std::array<uint32, 5> prim{};
    for (uint8_t i = 0; i < 5; ++i)
      prim[i] = character.GetPrimaryStat(i);
    player->SetPrimaryStats(prim);
  }
  {
    auto const *gt = _charService ? _charService->GetStatGameTables() : nullptr;
    float dimDodge = 0.f;
    float nondimDodge = 0.f;
    StatFormulas::ComputeDodgeContributionsFromAgility(
        character.GetLevel(), ToClassId(character.GetClass()), character.GetPrimaryStat(1),
        dimDodge, nondimDodge, gt);
    StatFormulas::AvoidanceClassParams const av =
        StatFormulas::AvoidanceParamsForClass(ToClassId(character.GetClass()));
    player->SetBaselineDodgePct(StatFormulas::AvoidanceAfterDiminishingReturns(
        av.dodgeCap, av.diminishingK, nondimDodge, dimDodge));
  }
  player->InitRegenContext(
      static_cast<uint8>(GetDefaultPlayerPowerType(character.GetClass())),
      character.GetPrimaryStat(4), character.GetLevel());
  runtime().AddPlayerToMap(_mapId, player);

  if (auto map = runtime().GetMap(_mapId)) {
    auto const now = std::chrono::steady_clock::now();
    if (_spellDefinitions) {
      std::vector<uint32_t> const passiveCandidates =
          CollectLoginPassiveSpellIds(_knownSpellIds, _spellDefinitions.get());
      player->SetKnownPermanentPassiveSpellIds(passiveCandidates);
      ApplyPassiveAurasForKnownSpellsOnMap(_mapId, map, guid, _playerLevel,
                                           passiveCandidates, now);
}
    SendActiveAurasOnMap(map, guid, now);
}

  SendLoginVerifyWorld(_mapId, move.x, move.y, move.z, move.orientation);

  TrySendFirstLoginOpeningCinematic(character);
}

void WorldSession::TrySendFirstLoginOpeningCinematic(Character const &character) {
  if (!character.IsFirstLogin() || _sentOpeningCinematic)
    return;
  uint32_t const seq =
      OpeningCinematicSequence(character.GetClass(), character.GetRace());
  if (seq == 0)
    return;
  SendTriggerCinematic(seq);
  _sentOpeningCinematic = true;
}

void WorldSession::SendNearbyCreatureCreatesInChunks(float x, float y) {
  auto map = runtime().GetMap(_mapId);
  if (!map)
    return;

  RebuildPlayerPhaseShiftFromActiveAuras();

  // Login log showed a single SMSG_UPDATE_OBJECT ~400KiB+ with full-ref spawns; the 4.3.4
  // client handles visibility poorly / shows nothing when one update blob is huge.
  constexpr uint32_t kMaxCreatureCreatesPerPacket = 48;
  uint16 const mapIdU16 = static_cast<uint16>(_mapId);
  UpdateData batch(mapIdU16);
  uint32_t inBatch = 0;
  _visibleCreatureGuids.clear();

  auto flushBatch = [this, mapIdU16, &batch, &inBatch]() {
    if (batch.GetBlockCount() == 0)
    return;
    WorldPacket pkt;
    batch.Build(pkt);
    SendPacket(pkt);
    batch = UpdateData(mapIdU16);
    inBatch = 0;
  };

  map->ForEachCreatureNear(x, y, 1, [&](std::shared_ptr<Creature> const &cr) {
    if (!IsCreatureVisibleToPlayer(*cr))
      return;
    _visibleCreatureGuids.insert(cr->GetGuid());
    auto const wire = ResolveCreatureWireFieldsForClient(*cr);
    auto npcFields = ws_obj::BuildMinimalNpcUnitCreateFields(
        cr->GetGuid(), cr->GetEntry(), wire.displayId, cr->GetLiveHealth(),
        cr->GetLiveMaxHealth(), cr->GetLevel(), wire.npcFlags, cr->GetFactionTemplate(),
        wire.unitFieldFlags, wire.unitFieldFlags2, cr->GetUnitDynamicFlags(),
        &cr->GetCombatStats());
    batch.AddCreateObject(cr->GetGuid(), TYPEID_UNIT, cr->GetPosition(),
                          npcFields);
    ++inBatch;
    if (inBatch >= kMaxCreatureCreatesPerPacket)
      flushBatch();
  });
      flushBatch();
}

void WorldSession::SendNearbyCreatureCreatesToSelf(float x, float y) {
  SendNearbyCreatureCreatesInChunks(x, y);
}

void WorldSession::LoginSendCreateUpdatesAndMutualVisibility(
    uint64 guid, Character const &character, MovementInfo const &move) {
  GtPlayerStatGameTables const *const statGt = _charService->GetStatGameTables();
  uint32_t const selfNextXp =
      character.GetLevel() < 85
          ? _charService->GetXpToNextLevelForLevel(character.GetLevel())
          : 0u;
  // Now that the player is on the map, send create/update data and "after add" packets
  UpdateData update(_mapId);
  auto selfFields = ws_obj::BuildPlayerUpdateFields(
      guid, character, statGt, selfNextXp, TryLivePlayerHealth(_mapId, guid),
      TryLivePlayerPower1(_mapId, guid), _knownSkills);
  MergeGmAppearanceIntoPlayerFields(selfFields, GetGmAppearanceForPlayerUpdates());
  ws_obj::ApplyMovementHintsToPlayerCreateFields(selfFields, move);
  update.AddCreateObject(guid, TYPEID_PLAYER, move, selfFields);

  MovementInfo itemMove{};
  for (size_t slot = 0; slot < kEquipmentSlotCount; ++slot) {
    uint32 const itemGuidLow = character.GetVisibleItemGuidLow(slot);
    uint32 const entry = character.GetVisibleItemEntry(slot);
    if (itemGuidLow == 0 || entry == 0)
        continue;
    uint64 const itemOg = MakeItemObjectGuid(itemGuidLow);
    update.AddCreateObject(
        itemOg, TYPEID_ITEM, itemMove,
        ws_obj::BuildItemCreateFields(itemOg, guid, entry,
                              character.GetVisibleItemStackCount(slot)));
}
  for (size_t pi = 0; pi < kPackSlotCount; ++pi) {
    uint32 const itemGuidLow = character.GetPackItemGuidLow(pi);
    uint32 const entry = character.GetPackItemEntry(pi);
    if (itemGuidLow == 0 || entry == 0)
        continue;
    uint64 const itemOg = MakeItemObjectGuid(itemGuidLow);
    update.AddCreateObject(
        itemOg, TYPEID_ITEM, itemMove,
        ws_obj::BuildItemCreateFields(itemOg, guid, entry,
                              character.GetPackItemStackCount(pi)));
}

  WorldPacket updatePacket(SMSG_UPDATE_OBJECT);
  update.Build(updatePacket);
  SendPacket(updatePacket);
  RebuildPlayerPhaseShiftFromActiveAuras();
  SendPlayerPhaseShiftToClient();
  SendNearbyCreatureCreatesInChunks(move.x, move.y);
  SendQuestGiverStatusMultipleNearby();

  // Other logged-in players see this client; this client sees them (same map).
  if (auto map = runtime().GetMap(_mapId)) {
    map->ForEachPlayer([this, guid, &character, &move, statGt, selfNextXp](
                           std::shared_ptr<Player> const &other) {
      if (!other || other->GetGuid() == guid)
    return;
      PlayerGmAppearanceForUpdates const newPlayerGm =
          GetGmAppearanceForPlayerUpdates();
      if (auto n = other->GetNotifier()) {
        ws_obj::SendPlayerCreateToNotifier(
            n, _mapId, guid, character, move, newPlayerGm, statGt, selfNextXp,
            TryLivePlayerHealth(_mapId, guid), TryLivePlayerPower1(_mapId, guid));
}
      if (auto otherCh = _charService->GetCharacterByGuid(other->GetGuid())) {
        PlayerGmAppearanceForUpdates otherGm{};
        if (auto ows =
                std::dynamic_pointer_cast<WorldSession>(other->GetNotifier())) {
          otherGm = ows->GetGmAppearanceForPlayerUpdates();
}
        uint32_t const otherNext =
            otherCh->GetLevel() < 85
                ? _charService->GetXpToNextLevelForLevel(otherCh->GetLevel())
          : 0u;
        ws_obj::SendPlayerCreateToNotifier(
            std::static_pointer_cast<IMapNotifier>(shared_from_this()), _mapId,
            other->GetGuid(), *otherCh, other->GetPosition(), otherGm, statGt,
            otherNext, TryLivePlayerHealth(_mapId, other->GetGuid()),
            TryLivePlayerPower1(_mapId, other->GetGuid()));
}
  });
}
}

void WorldSession::LoginFinalizeWorldEntry(uint64 guid) {
  // Equivalent of Player::SendInitialPacketsAfterAddToMap: world states, then
  // ResetTimeSync + SendTimeSync (WorldSession.cpp in reference).
  SendInitWorldStates(_mapId, _zoneId);
  WorldPacket timeSync(SMSG_TIME_SYNC_REQ);
  timeSync.Append<uint32>(_timeSyncNextCounter++);
  SendPacket(timeSync);
  // Match next time-sync is periodic via timer, not on each RESP.
  CancelPeriodicTimeSync();
  SchedulePeriodicTimeSync();
  SendLoadCUFProfiles();

  LOG_DEBUG("Player {} finished world entry (map {})", guid, _mapId);

  if (auto host = runtime().GetScriptHost()) {
    host->FireEvent("player_login", guid);
}

  if (_onlineCharRegistry) {
    if (auto ch = _charService->GetCharacterByGuid(guid)) {
      _activeCharacterName = ch->GetName();
      _onlineCharRegistry->Register(
          _activeCharacterName, guid,
          std::weak_ptr<ICommandSession>(
              std::static_pointer_cast<ICommandSession>(shared_from_this())),
          FactionSideFromPlayableRace(ch->GetRace()));
}
}

  if (_gmFlyEnabled || std::fabs(_gmRunSpeed - 7.0f) > 1e-3f)
    PublishGmMovementPacketsIfInWorld();

  // If login spawn already has swim flags (rare persisted state), show breath UI immediately.
  UpdateBreathFromSwimmingState(MovementIsSwimming(_position));
  _movementAnimTierSent =
      MovementAnimTierForLiquid(_position, MovementIsSwimming(_position));

  SendClientActiveSpellCooldowns();
  SendClientActiveCategoryCooldowns();

  // Client may reset bars after player UPDATE_OBJECT; resend once spellbook is active.
  LoadActionButtonsForCharacter(static_cast<uint32_t>(guid));
  SendActionButtons(1);
  SendActionBarTogglesUpdate();

  // Login create update zeros aura-derived stat/AP fields; re-sync from active auras.
  if (auto map = runtime().GetMap(_mapId)) {
    if (_playerLevel > 0)
      BroadcastPlayerAuraStatBonusOnMap(_mapId, map, guid, _playerLevel);
}
}

} // namespace Firelands
