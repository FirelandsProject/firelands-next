#ifndef FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_WORLD_SESSION_H
#define FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_WORLD_SESSION_H

#include <application/ports/IAuthSession.h>
#include <application/ports/ICommandService.h>
#include <application/ports/ICommandSession.h>
#include <application/ports/IMapNotifier.h>
#include <application/services/AuthService.h>
#include <application/services/CharacterService.h>
#include <application/spell/SpellManager.h>
#include <domain/repositories/INpcTemplateSearchRepository.h>
#include <boost/asio.hpp>
#include <deque>
#include <memory>
#include <shared/network/BitReader.h>
#include <shared/network/BitWriter.h>
#include <shared/network/ByteBuffer.h>
#include <shared/network/MovementInfo.h>
#include <shared/network/ServerPacket.h>
#include <shared/network/WorldCrypt.h>
#include <shared/network/AccountDataTypes.h>
#include <shared/dbc/ItemDbHotfixStore.h>
#include <shared/dbc/LanguagesDbc.h>
#include <shared/game/AccessLevel.h>
#include <shared/game/PlayerGmAppearance.h>
#include <domain/models/GossipMenu.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <array>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Firelands {

class UpdateData;

using boost::asio::ip::tcp;

/// One row from the zlib-compressed secure-addon block in CMSG_AUTH_SESSION (4.3.4).
struct AuthSecureAddonEntry {
  std::string name;
  bool hasKey = false;
};

class MySqlAccountDataRepository;
class IRealmRepository;
class OnlineCharacterSessionRegistry;
class GmTicketService;
class ISpellDefinitionStore;
class FactionTemplateDbc;
class IGossipRepository;
class INpcTextRepository;

class WorldSession : public IAuthSession,
                     public IMapNotifier,
                     public ICommandSession,
                     public std::enable_shared_from_this<WorldSession> {
public:
  explicit WorldSession(
      tcp::socket socket, std::shared_ptr<AuthService> authService,
      std::shared_ptr<CharacterService> charService,
      std::shared_ptr<ICommandService> commandService,
      std::shared_ptr<MySqlAccountDataRepository> accountDataRepo,
      std::shared_ptr<LanguagesDbc const> languagesDbc = nullptr,
      std::shared_ptr<ISpellDefinitionStore const> spellDefinitions = nullptr,
      std::shared_ptr<IRealmRepository> realmRepo = nullptr,
      std::shared_ptr<OnlineCharacterSessionRegistry> onlineCharRegistry =
          nullptr,
      std::shared_ptr<GmTicketService> gmTicketService = nullptr,
      std::shared_ptr<ItemDbHotfixStore const> itemDbHotfix = nullptr,
      std::shared_ptr<SpellManager> spellManager = nullptr,
      std::shared_ptr<INpcTemplateSearchRepository const> npcTemplateSearch =
          nullptr,
       std::shared_ptr<FactionTemplateDbc const> factionTemplateDbc = nullptr,
       std::shared_ptr<IGossipRepository> gossipRepo = nullptr,
       std::shared_ptr<INpcTextRepository> npcTextRepo = nullptr);

  ~WorldSession();

  void Start();

  void SendPacket(WorldPacket &packet) override;
  void SendPacket(ServerPacket *packet);
  void SendPacket(ByteBuffer &buffer) override;
  void SendAuthChallenge();
  void Close() override;
  std::string GetIpAddress() const override;

  // Command Support
  void SendNotification(const std::string &message) override;
  void SendScreenNotification(std::string const &message) override;
  void TeleportTo(uint32 mapId, float x, float y, float z,
                  float orientation = 0.0f) override;

  uint64 GetGuid() const override { return _playerGuid; }
  const MovementInfo &GetPosition() const override { return _position; }
  uint32 GetMapId() const override { return _mapId; }
  AccessLevel GetAccountAccessLevel() const override {
    return _accountAccessLevel;
  }

  void RequestDisconnect(std::string const &reason) override;

  void SetGmTagEnabled(bool on) override;
  void SetDndEnabled(bool on) override;
  void SetDevTagEnabled(bool on) override;
  void SetGmVisibleToPlayers(bool visible) override;
  void SetGmFlyEnabled(bool on) override;
  void SetGmRunSpeed(float speed) override;

  bool GmLearnSpell(uint32 spellId) override;
  bool GmModifyMoneyCopper(int64 deltaCopper) override;
  bool GmAddItem(uint32 itemEntry, uint32 count) override;
  bool GmRemoveItem(uint32 itemEntry, uint32 count) override;
  bool GmSetLevel(uint8 level) override;

  bool GmSpawnNpc(uint32 creatureEntry, uint32 displayId,
                  uint32 factionTemplateOrZeroDefault = 0) override;
  bool GmDeleteNpcByObjectGuid(uint64 objectGuid) override;

  bool GmSetForcedFactionReaction(uint32 factionDbcId,
                                  uint8 reputationRank) override;
  bool GmClearForcedFactionReaction(uint32 factionDbcId) override;
  bool GmClearAllForcedFactionReactions() override;
  bool GmSetOwnFactionTemplate(uint32 factionTemplate) override;
  bool GmSetSelectedCreatureFactionTemplate(uint32 factionTemplate) override;

  uint64_t GetClientSelectionGuid() const override { return _clientSelectionGuid; }
  uint64_t GetActiveCharacterObjectGuid() const override { return _playerGuid; }

  void SendGmResponseReceived(uint32_t ticketId,
                              std::string const &playerMessage,
                              std::string const &gmResponse) override;
  uint32_t GetAccountId() const override { return _accountId; }

  void OpenGmMailboxUi() override;

  bool GmNpcSearchPrintResults(std::string const &nameQuery) override;

  PlayerGmAppearanceForUpdates GetGmAppearanceForPlayerUpdates() const {
    return _gmAppearance;
  }

 private:
  void ResetGmStateForLogout();
  void PublishGmVisualPatchIfInWorld();
  void PublishGmMovementPacketsIfInWorld();
  // Core Network Logic
  void DoRead();
  void DoWrite();
  void QueueOutgoing(std::shared_ptr<std::vector<uint8>> buffer);
  void HandlePacket(ByteBuffer &buffer);
  void ProcessPacket(WorldPacket &packet);

  // Client Packet Handlers (CMSG)
  void HandleAuthSession(WorldPacket &packet);
  void HandleAuthSessionScattered(WorldPacket &packet, uint8 *digest,
                                  std::vector<uint8> &localChallenge,
                                  uint16 &build, uint32 &realmId,
                                  int32 &loginServerId);
  void HandleAuthSessionStandard(WorldPacket &packet, uint16 &build,
                                 uint8 *digest,
                                 std::vector<uint8> &localChallenge,
                                 uint32 &realmId);
  void HandleCharEnum(WorldPacket &packet);
  void HandleCharCreate(WorldPacket &packet);
  void HandleCharDelete(WorldPacket &packet);
  void HandlePlayerLogin(WorldPacket &packet);
  void HandleLogoutRequest(WorldPacket &packet);
  void HandleLogoutCancel(WorldPacket &packet);
  void HandleNameQuery(WorldPacket &packet);
  void HandleCreatureQuery(WorldPacket &packet);
  void HandleQueryTime(WorldPacket &packet);
  void HandlePlayedTime(WorldPacket &packet);
  void HandleMovement(WorldPacket &packet);
  void HandleMoveTeleportAck(WorldPacket &packet);
  void HandlePing(WorldPacket &packet);
  void HandleSetSelection(WorldPacket &packet);
  void HandleTimeSyncResp(WorldPacket &packet);
  void HandleMoveTimeSkipped(WorldPacket &packet);
  void HandleMessageChat(WorldPacket &packet);
  void HandleAddonMessageChat(WorldPacket &packet);
  void HandleRealmSplit(WorldPacket &packet);
  void HandleReadyForAccountDataTimes(WorldPacket &packet);
  void HandleUpdateAccountData(WorldPacket &packet);
  void HandleRequestAccountData(WorldPacket &packet);
  void HandleGossipHello(WorldPacket &packet);
  void HandleGossipSelectOption(WorldPacket &packet);
  void HandleNpcTextQuery(WorldPacket &packet);
  void HandleQueryNextMailTime(WorldPacket &packet);
  void HandleMailGetList(WorldPacket &packet);
  void HandleCalendarGetNumPending(WorldPacket &packet);
  void HandleZoneUpdate(WorldPacket &packet);
  void HandleGuildBankRemainingWithdrawMoneyQuery(WorldPacket &packet);
  void HandleLfgGetStatus(WorldPacket &packet);
  void HandleLfgLockInfoRequest(WorldPacket &packet);
  void HandleRequestCemeteryList(WorldPacket &packet);
  void HandleCastSpell(WorldPacket &packet);
  void HandleSwapInvItem(WorldPacket &packet);
  void HandleSwapItem(WorldPacket &packet);
  void HandleDbQueryBulk(WorldPacket &packet);
  void HandleAutoEquipItem(WorldPacket &packet);
  void HandleAutoEquipItemSlot(WorldPacket &packet);
  void HandleUseItem(WorldPacket &packet);
  void HandleDestroyItem(WorldPacket &packet);
  void HandleGmTicketCreate(WorldPacket &packet);
  void HandleGmTicketUpdateText(WorldPacket &packet);
  void HandleGmTicketDelete(WorldPacket &packet);
  void HandleGmTicketGetTicket(WorldPacket &packet);
  void HandleGmTicketSystemStatus(WorldPacket &packet);
  void HandleGmTicketResponseResolve(WorldPacket &packet);
  void HandleTutorialFlag(WorldPacket &packet);
  void HandleTutorialClear(WorldPacket &packet);
  void HandleTutorialReset(WorldPacket &packet);
  void HandleCompleteMovie(WorldPacket &packet);
  void HandleOpeningCinematic(WorldPacket &packet);
  void HandleCompleteCinematic(WorldPacket &packet);
  void HandleNextCinematicCamera(WorldPacket &packet);

  // Server Packet Senders (SMSG)
  void SendAuthResponse();
  void SendAddonInfo();
  void SendClientCacheVersion(uint32 version = 0);
  /// Character-select / pre-player socket parity (Trinity `SendTutorialsData` without player).
  void SendTutorialFlagsUnauthenticated();
  /// In-world mask (`SMSG_TUTORIAL_FLAGS`): set bits mark completed tutorial triggers.
  void SendTutorialMask(std::array<uint32_t, Character::kTutorialMaskInts> const &mask);
  void SendTriggerMovie(uint32_t movieId);
  void SendTriggerCinematic(uint32_t cinematicSequenceId);
  void SendAccountDataTimes(uint32 mask);
  void ReloadGlobalAccountDataFromDb();
  void ReloadCharacterAccountDataFromDb(uint32 characterGuid);
  void SendFeatureSystemStatus();
  void SendRealmSplit(uint32 realmId);
  void SendLoginSetTimeSpeed(float speed = 0.01666667f);
  void SendLearnedDanceMoves();
  void SendMotd();
  void SendInitialObjectUpdate(uint64 guid);
  /// Matches WorldPackets::Spells::SendKnownSpells (Cataclysm 4.3.4).
  void SendKnownSpells(bool initialLogin, std::vector<uint32> const &spellIds);
  /// Same payload as Trinity `Player::LearnSpell` → `SMSG_LEARNED_SPELL`.
  void SendLearnedSpell(uint32 spellId);
  void SendUnlearnSpellsEmpty();
  void SendDungeonDifficulty(bool inGroup = false);
  void SendHotfixNotifyBlobEmpty();
  void SendContactListEmpty();
  void SendAllAchievementDataEmpty();
  void SendEquipmentSetListEmpty();
  void SendInitialActionButtons();
  void SendInitWorldStates(uint32 mapId, uint32 zoneId = 0, uint32 areaId = 0);
  void SendSetupCurrency();
  void SendClientControlUpdate(uint64 guid);
  void SendBindPointUpdate();
  void SendWorldServerInfo();
  void SendLoadCUFProfiles();
  void SendForcedReactions();
  void SendSetProficiency(uint8 itemClass, uint32 itemMask);
  void SendTalentsInfo();
  void SendInitialFactions();
  void SendLoginVerifyWorld(uint32 mapId, float x, float y, float z, float o);
  void SendMailListToClient(uint32_t characterGuid);
  void SendQueryTimeResponse();

  // Gossip
  void SendGossipMessage(uint64_t npcGuid, uint32_t menuId, uint32_t textId,
                         std::vector<GossipMenuItem> const &items);
  void SendGossipComplete();
  std::optional<uint32_t> TryResolveCreatureTemplateEntry(uint64_t npcGuid) const;
  bool TrySendDatabaseGossipMenu(uint64_t npcGuid, uint32_t templateEntry);

  // Helpers
  /// Trinity schedules the next time-sync ~5s after SendTimeSync; never chains on
  /// every CMSG_TIME_SYNC_RESP (that floods the client and breaks map loading).
  void SchedulePeriodicTimeSync();
  void CancelPeriodicTimeSync();
  void ResetBreathMirrorState();
  void UpdateBreathFromSwimmingState(bool swimming);
  void SendStartMirrorTimerPacket(int32_t timerType, int32_t value, int32_t maxValue,
                                  int32_t scale, bool paused, int32_t spellId);
  void SendStopMirrorTimerPacket(int32_t timerType);

  /// CMSG_PLAYER_LOGIN sub-steps (keeps `HandlePlayerLogin` readable).
  void LoginSendAccountDataAndPreMapPackets(uint64 guid, Character const &character);
  void LoginBuildKnownSpellsAndSendSpellbook(Character const &character);
  void LoginSendMotdAndMetaPackets();
  void LoginResolveMapPosition(uint64 guid, Character const &character,
                                MovementInfo &outMove);
  void LoginSpawnInWorld(uint64 guid, Character const &character,
                         MovementInfo const &move);
  void LoginSendCreateUpdatesAndMutualVisibility(uint64 guid, Character const &character,
                                                 MovementInfo const &move);
  /// Sends UNIT CREATE blocks for creatures in the 3×3 grid neighbourhood using multiple
  /// `SMSG_UPDATE_OBJECT` packets (dense zones exceed safe single-packet sizes on 4.3.4).
  void SendNearbyCreatureCreatesInChunks(float x, float y);
  /// After same-map teleport ACK: client needs CREATE for units near the new cell.
  void SendNearbyCreatureCreatesToSelf(float x, float y);
  void LoginFinalizeWorldEntry(uint64 guid);
  void TrySendFirstLoginOpeningCinematic(Character const &character);
  void UnregisterFromOnlineCharacterRegistryIfNeeded();
  /// Persists position, `player_logout`, removes from map and online registry, clears
  /// in-world fields. Requires `_playerGuid != 0`.
  void FinalizeWorldExit();
  void PublishSelfCoinageUpdate();
  void PublishUnitFactionTemplateUpdate(uint64 unitGuid, uint32 factionTemplate);

  /// Payload for `SpellCastOutcome::SpellStartDeferred` timer completion (`SMSG_SPELL_GO` + effects).
  struct PendingSpellCastFinish {
    uint32 mapId = 0;
    uint64 casterGuid = 0;
    uint8 castId = 0;
    uint32 spellId = 0;
    uint32 targetFlags = 0;
    uint64 targetUnitGuid = 0;
    uint64 hitGuid = 0;
    bool hasDirectHealthEffect = false;
    uint64 directHealthTargetGuid = 0;
    int32 directHealthDelta = 0;
    int32 power1Delta = 0;
    uint32 spellCooldownDurationMs = 0;
    uint32 spellCategoryCooldownGroup = 0;
    uint32 spellCategoryCooldownDurationMs = 0;
  };

  void CancelPendingClientSpellCast();
  void ScheduleDeferredSpellCastCompletion(SpellCastOutcome const &out);
  void CompleteDeferredSpellCast(PendingSpellCastFinish const &finish);

  tcp::socket _socket;
  std::shared_ptr<AuthService> _authService;
  std::shared_ptr<CharacterService> _charService;
  std::shared_ptr<ICommandService> _commandService;
  std::shared_ptr<MySqlAccountDataRepository> _accountDataRepo;
  std::shared_ptr<LanguagesDbc const> _languagesDbc;
  std::shared_ptr<ISpellDefinitionStore const> _spellDefinitions;
  std::shared_ptr<IRealmRepository> _realmRepo;
  std::shared_ptr<OnlineCharacterSessionRegistry> _onlineCharRegistry;
  std::shared_ptr<GmTicketService> _gmTicketService;
  std::shared_ptr<ItemDbHotfixStore const> _itemDbHotfix;
  std::shared_ptr<SpellManager> _spellManager;
  std::shared_ptr<INpcTemplateSearchRepository const> _npcTemplateSearch;
  std::shared_ptr<FactionTemplateDbc const> _factionTemplateDbc;
  std::shared_ptr<IGossipRepository> _gossipRepo;
  std::shared_ptr<INpcTextRepository> _npcTextRepo;

  /// Filled when the character is registered for console targeting; empty at
  /// character select / disconnected.
  std::string _activeCharacterName;
  std::array<AccountDataSlot, NUM_ACCOUNT_DATA_TYPES> _accountData{};
  uint32_t _activeCharacterGuid = 0;
  /// Per-character account blobs edited at character select (no guid yet); flushed on login.
  uint32_t _preLoginPerCharAccountDirtyMask = 0;
  bool _initialized = false;
  uint32 _serverSeed;
  uint32 _accountId = 0;
  AccessLevel _accountAccessLevel = AccessLevel::Player;
  uint64 _playerGuid = 0;
  /// Latest `CMSG_SET_SELECTION` unit (0 = cleared / unknown).
  uint64_t _clientSelectionGuid = 0;
  uint8 _playerRace = 0;
  /// Persisted copper; mirrored on logout and after `.money` GM commands.
  uint32_t _moneyCopper = 0;
  /// Persisted experience (`characters.xp`); mirrored on logout and GM level.
  uint32_t _playerXp = 0;
  bool _sentOpeningCinematic = false;
  std::array<uint32_t, Character::kTutorialMaskInts> _tutorialInts{};
  uint32 _mapId = 0;
  uint32 _zoneId = 0;
  MovementInfo _position;
  uint8 _readBuffer[2048];
  ByteBuffer _inBuffer;
  WorldCrypt _crypt;
  uint8 _decHeader[6]{};
  bool _headerDecrypted = false;

  // Write queue: serializes async_write calls to prevent interleaving
  std::deque<std::shared_ptr<std::vector<uint8>>> _writeQueue;
  bool _writing = false;

  // Diagnostics: last SMSG sent (helps correlate client disconnect/crash)
  uint32 _lastSentOpcode = 0;
  uint32 _lastSentPayloadSize = 0;

  /// Monotonic counter for SMSG_TIME_SYNC_REQ (see WorldSession::SendTimeSync in reference).
  uint32 _timeSyncNextCounter = 0;

  boost::asio::steady_timer _timeSyncPeriodicTimer;
  boost::asio::steady_timer _pendingSpellCastTimer;

  /// Filled while handling CMSG_AUTH_SESSION; consumed by SendAddonInfo (SMSG_ADDON_INFO).
  std::vector<AuthSecureAddonEntry> _authSecureAddons;

  /// Known spells for the logged-in character (mirrors `SMSG_SEND_KNOWN_SPELLS` payload).
  std::vector<uint32> _knownSpells;
  /// Same ids as `_knownSpells` for O(1) lookups (`SpellManager`, GM helpers).
  std::unordered_set<uint32> _knownSpellIds;
  std::chrono::steady_clock::time_point _gcdReady{};
  /// Phase E: per-spell recovery (`SpellCooldowns.dbc` RecoveryTime) until instant.
  std::unordered_map<uint32, std::chrono::steady_clock::time_point> _spellCooldownUntil;
  /// Phase E: shared category recovery (`SpellCooldowns` + `SpellCategories.dbc` group).
  std::unordered_map<uint32, std::chrono::steady_clock::time_point>
      _spellCategoryCooldownUntil;

  PlayerGmAppearanceForUpdates _gmAppearance{};
  bool _gmFlyEnabled = false;
  float _gmRunSpeed = 7.0f;
  uint32 _moveCounterForGmPackets = 0;

  /// Near teleport (same map): waiting for `MSG_MOVE_TELEPORT_ACK` before committing pos.
  bool _awaitingTeleportNear = false;
  uint32 _teleportAckExpectedIndex = 0;
  float _teleportPendingX = 0.f;
  float _teleportPendingY = 0.f;
  float _teleportPendingZ = 0.f;
  float _teleportPendingO = 0.f;

  /// Underwater breath (`MirrorTimerType::Breath`); driven from movement `MOVEMENTFLAG_SWIMMING`.
  bool _breathMirrorActive = false;
  int32_t _breathRemainingMs = 0;
  std::optional<std::chrono::steady_clock::time_point> _breathLastMonotonicTick;
  int32_t _breathLastSentValueMs = -1;

  /// Faction.dbc id → forced `ReputationRank` for `SMSG_SET_FORCED_REACTIONS` (quest/script overrides).
  std::map<uint32, uint32> _forcedFactionReactions;

  /// Set when this session sends `SMSG_GOSSIP_MESSAGE` (Lua may add APIs later).
  bool _gossipMenuSent = false;
};

} // namespace Firelands

#endif
