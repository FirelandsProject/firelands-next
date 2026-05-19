#include <application/ports/IMapNotifier.h>
#include <application/services/OnlineCharacterSessionRegistry.h>
#include <application/services/WorldService.h>
#include <infrastructure/persistence/MySqlAccountDataRepository.h>
#include <domain/models/Character.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <domain/world/Creature.h>
#include <domain/world/Player.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionMovementChecks.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/Logger.h>
#include <shared/dbc/GtPlayerStatGameTables.h>
#include <shared/game/ChatLanguages.h>
#include <shared/game/PlayerFactionTeam.h>
#include <shared/game/PlayerGmAppearance.h>
#include <shared/game/StarterOpeningCinematic.h>
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
  auto map = WorldService::Instance().GetMap(mapId);
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
  auto map = WorldService::Instance().GetMap(mapId);
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
  _sentOpeningCinematic = false;
  _tutorialInts = character.GetTutorialMask();
  _playerRace = character.GetRace();
  _playerClass = character.GetClass();
  _moneyCopper = character.GetMoney();
  _playerXp = character.GetXp();

  LOG_DEBUG("PlayerLogin: Account={} GUID={} Name='{}' Level={} Map={}",
            _accountId, guid, character.GetName(), character.GetLevel(),
            character.GetMapId());

  LoginSendAccountDataAndPreMapPackets(guid, character);
  LoginBuildKnownSpellsAndSendSpellbook(character);
  LoginSendMotdAndMetaPackets();

  MovementInfo move{};
  LoginResolveMapPosition(guid, character, move);

  LoginSpawnInWorld(guid, character, move);
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
  SendSetProficiency(1, 0xFFFFFFFF);
  SendSetProficiency(2, 0xFFFFFFFF);
}

void WorldSession::LoginBuildKnownSpellsAndSendSpellbook(Character const &character) {
  {
    // Language passives must appear early in `SMSG_SEND_KNOWN_SPELLS`. Some 4.3.4
    // clients stop applying the list if an early spell id is unknown to the
    // client DB, which leaves language passives unlearned and blocks `/say`
    // before `CMSG_MESSAGECHAT_SAY` is ever sent.
    std::vector<uint32> spells;
    AppendRacialLanguageSpells(character.GetRace(), spells);
    std::vector<uint32_t> const fromDb = _charService->GetStarterSpells(
        static_cast<uint8_t>(character.GetRace()),
        static_cast<uint8_t>(character.GetClass()));
    auto pushUnique = [&spells](uint32 sid) {
      if (sid == 0)
        return;
      if (std::find(spells.begin(), spells.end(), sid) == spells.end())
        spells.push_back(sid);
    };
    if (!fromDb.empty()) {
      spells.reserve(spells.size() + fromDb.size());
      for (uint32_t sid : fromDb)
        pushUnique(static_cast<uint32>(sid));
    } else {
      for (uint32 sid : ws_obj::BuildDefaultKnownSpells(character.GetClass()))
        pushUnique(sid);
    }
    // Strip ids from wrong client eras that break spellbook application on 4.3.4.
    for (auto it = spells.begin(); it != spells.end();) {
      uint32 const sid = *it;
      if (sid >= 86450u && sid <= 86550u)
        it = spells.erase(it);
      else
        ++it;
    }
    if (_spellDefinitions) {
      for (auto it = spells.begin(); it != spells.end();) {
        uint32 const sid = *it;
        // Never drop passive "Language *" spells: wrong-era DBC would break /say.
        if (!IsLanguagePassiveSpell(sid) && !_spellDefinitions->HasSpell(sid)) {
          LOG_WARN(
              "Spell id {} not in Spell.dbc; omitted from known spells (race={} "
              "class={})",
              sid, static_cast<uint32>(character.GetRace()),
              static_cast<uint32>(character.GetClass()));
          it = spells.erase(it);
        } else
          ++it;
      }
    }
    for (uint32_t sid : _charService->GetCharacterSpellIds(character.GetGuid())) {
      uint32 const u = static_cast<uint32>(sid);
      if (u == 0)
        continue;
      if (_spellDefinitions && !IsLanguagePassiveSpell(u) &&
          !_spellDefinitions->HasSpell(u)) {
        continue;
      }
      if (std::find(spells.begin(), spells.end(), u) == spells.end())
        spells.push_back(u);
    }
    EnsureRacialLanguageSpells(static_cast<uint8>(character.GetRace()), spells);
    PrioritizeDefaultLanguageSpell(static_cast<uint8>(character.GetRace()),
                                   spells);
    _knownSpells = std::move(spells);
    RebuildKnownSpellIdSet(_knownSpells, _knownSpellIds);
    uint32 const defaultLang = DefaultLanguageForRace(character.GetRace());
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
                static_cast<uint32>(character.GetClass()), defaultLang,
                defaultLangSpell,
                PlayerKnowsLanguage(_knownSpellIds, defaultLang) ? 1 : 0,
                _knownSpells.size(), ids);
    }
  }
  _gcdReady = {};
  _spellCooldownUntil.clear();
  _spellCategoryCooldownUntil.clear();
  // At world login this packet must initialize client spellbook state,
  // including passive language spells. Existing characters may have
  // `firstLogin = false`, but the client still expects InitialLogin=1 here.
  SendKnownSpells(true, _knownSpells);
  SendUnlearnSpellsEmpty();
  SendTalentsInfo();
  SendInitialActionButtons();
  SendInitialFactions();
  SendContactListEmpty();
  SendForcedReactions();
  SendSetupCurrency();
  SendAllAchievementDataEmpty();
  SendLoginSetTimeSpeed();
  SendEquipmentSetListEmpty();
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
  WorldService::Instance().AddPlayerToMap(_mapId, player);

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
  auto map = WorldService::Instance().GetMap(_mapId);
  if (!map)
    return;

  // Login log showed a single SMSG_UPDATE_OBJECT ~400KiB+ with full-ref spawns; the 4.3.4
  // client handles visibility poorly / shows nothing when one update blob is huge.
  constexpr uint32_t kMaxCreatureCreatesPerPacket = 48;
  uint16 const mapIdU16 = static_cast<uint16>(_mapId);
  UpdateData batch(mapIdU16);
  uint32_t inBatch = 0;

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
    const uint32_t npcFlags = ResolveEffectiveNpcFlagsForCreature(*cr);
    auto npcFields = ws_obj::BuildMinimalNpcUnitCreateFields(
        cr->GetGuid(), cr->GetEntry(), cr->GetDisplayId(), cr->GetLiveHealth(),
        cr->GetLiveMaxHealth(), cr->GetLevel(), npcFlags, cr->GetFactionTemplate());
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
      TryLivePlayerPower1(_mapId, guid));
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
  SendNearbyCreatureCreatesInChunks(move.x, move.y);
  SendQuestGiverStatusMultipleNearby();

  // Other logged-in players see this client; this client sees them (same map).
  if (auto map = WorldService::Instance().GetMap(_mapId)) {
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
  // Match Trinity: next time-sync is periodic via timer, not on each RESP.
  CancelPeriodicTimeSync();
  SchedulePeriodicTimeSync();
  SendLoadCUFProfiles();

  LOG_DEBUG("Player {} finished world entry (map {})", guid, _mapId);

  if (auto host = WorldService::Instance().GetScriptHost()) {
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
}

} // namespace Firelands
