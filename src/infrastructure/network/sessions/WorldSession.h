#ifndef FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_WORLD_SESSION_H
#define FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_WORLD_SESSION_H

#include <application/ports/IAuthSession.h>
#include <application/ports/ICommandService.h>
#include <application/ports/ICommandSession.h>
#include <application/ports/IMapNotifier.h>
#include <application/services/AuthService.h>
#include <application/services/CharacterService.h>
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
#include <shared/dbc/LanguagesDbc.h>
#include <shared/game/AccessLevel.h>
#include <shared/game/PlayerGmAppearance.h>
#include <shared/dbc/SpellDbc.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <array>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace Firelands {

class Character;

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
      std::shared_ptr<SpellDbc const> spellDbc = nullptr,
      std::shared_ptr<IRealmRepository> realmRepo = nullptr,
      std::shared_ptr<OnlineCharacterSessionRegistry> onlineCharRegistry =
          nullptr,
      std::shared_ptr<GmTicketService> gmTicketService = nullptr);

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
  bool GmSetLevel(uint8 level) override;

  void SendGmResponseReceived(uint32_t ticketId,
                              std::string const &playerMessage,
                              std::string const &gmResponse) override;
  uint32_t GetAccountId() const override { return _accountId; }

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
  void HandleQueryTime(WorldPacket &packet);
  void HandlePlayedTime(WorldPacket &packet);
  void HandleMovement(WorldPacket &packet);
  void HandlePing(WorldPacket &packet);
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
  void HandleQueryNextMailTime(WorldPacket &packet);
  void HandleCalendarGetNumPending(WorldPacket &packet);
  void HandleZoneUpdate(WorldPacket &packet);
  void HandleGuildBankRemainingWithdrawMoneyQuery(WorldPacket &packet);
  void HandleLfgGetStatus(WorldPacket &packet);
  void HandleLfgLockInfoRequest(WorldPacket &packet);
  void HandleRequestCemeteryList(WorldPacket &packet);
  void HandleCastSpell(WorldPacket &packet);
  void HandleSwapInvItem(WorldPacket &packet);
  void HandleSwapItem(WorldPacket &packet);
  void HandleGmTicketCreate(WorldPacket &packet);
  void HandleGmTicketUpdateText(WorldPacket &packet);
  void HandleGmTicketDelete(WorldPacket &packet);
  void HandleGmTicketGetTicket(WorldPacket &packet);
  void HandleGmTicketSystemStatus(WorldPacket &packet);
  void HandleGmTicketResponseResolve(WorldPacket &packet);

  // Server Packet Senders (SMSG)
  void SendAuthResponse();
  void SendAddonInfo();
  void SendClientCacheVersion(uint32 version = 0);
  void SendTutorialFlags();
  void SendAccountDataTimes(uint32 mask);
  void ReloadGlobalAccountDataFromDb();
  void ReloadCharacterAccountDataFromDb(uint32 characterGuid);
  void SendFeatureSystemStatus();
  void SendRealmSplit(uint32 realmId);
  void SendLoginSetTimeSpeed(float speed = 0.01666667f);
  void SendLearnedDanceMoves();
  void SendMotd();
  void SendInitialObjectUpdate(uint64 guid);
  /// Matches WorldPackets::Spells::SendKnownSpells (FirelandsCore / TCPP 4.3.4).
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
  void SendQueryTimeResponse();

  // Helpers
  /// Trinity schedules the next time-sync ~5s after SendTimeSync; never chains on
  /// every CMSG_TIME_SYNC_RESP (that floods the client and breaks map loading).
  void SchedulePeriodicTimeSync();
  void CancelPeriodicTimeSync();

  /// CMSG_PLAYER_LOGIN sub-steps (keeps `HandlePlayerLogin` readable).
  void LoginReadPackedPlayerGuid(WorldPacket &packet, uint64 &outGuid);
  void LoginSendAccountDataAndPreMapPackets(uint64 guid, Character const &character);
  void LoginBuildKnownSpellsAndSendSpellbook(Character const &character);
  void LoginSendMotdAndMetaPackets();
  void LoginResolveMapPosition(uint64 guid, Character const &character,
                                MovementInfo &outMove);
  void LoginSpawnInWorld(uint64 guid, MovementInfo const &move);
  void LoginSendCreateUpdatesAndMutualVisibility(uint64 guid, Character const &character,
                                                 MovementInfo const &move);
  void LoginFinalizeWorldEntry(uint64 guid);
  void UnregisterFromOnlineCharacterRegistryIfNeeded();
  /// Persists position, `player_logout`, removes from map and online registry, clears
  /// in-world fields. Requires `_playerGuid != 0`.
  void FinalizeWorldExit();
  void PublishSelfCoinageUpdate();

  tcp::socket _socket;
  std::shared_ptr<AuthService> _authService;
  std::shared_ptr<CharacterService> _charService;
  std::shared_ptr<ICommandService> _commandService;
  std::shared_ptr<MySqlAccountDataRepository> _accountDataRepo;
  std::shared_ptr<LanguagesDbc const> _languagesDbc;
  std::shared_ptr<SpellDbc const> _spellDbc;
  std::shared_ptr<IRealmRepository> _realmRepo;
  std::shared_ptr<OnlineCharacterSessionRegistry> _onlineCharRegistry;
  std::shared_ptr<GmTicketService> _gmTicketService;
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
  uint8 _playerRace = 0;
  /// Persisted copper; mirrored on logout and after `.money` GM commands.
  uint32_t _moneyCopper = 0;
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

  /// Filled while handling CMSG_AUTH_SESSION; consumed by SendAddonInfo (SMSG_ADDON_INFO).
  std::vector<AuthSecureAddonEntry> _authSecureAddons;

  /// Known spells for the logged-in character (mirrors `SMSG_SEND_KNOWN_SPELLS` payload).
  std::vector<uint32> _knownSpells;
  std::chrono::steady_clock::time_point _gcdReady{};

  PlayerGmAppearanceForUpdates _gmAppearance{};
  bool _gmFlyEnabled = false;
  float _gmRunSpeed = 7.0f;
  uint32 _moveCounterForGmPackets = 0;
};

} // namespace Firelands

#endif
