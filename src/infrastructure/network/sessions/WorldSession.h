#ifndef FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_WORLD_SESSION_H
#define FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_WORLD_SESSION_H

#include <application/ports/IAuthSession.h>
#include <application/ports/ICommandService.h>
#include <application/ports/ICommandSession.h>
#include <application/ports/IWorldRuntime.h>
#include <domain/ports/IMapNotifier.h>
#include <domain/repositories/INpcTemplateSearchRepository.h>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <shared/network/BitReader.h>
#include <shared/network/BitWriter.h>
#include <shared/network/ByteBuffer.h>
#include <shared/network/MovementInfo.h>
#include <shared/network/ServerPacket.h>
#include <shared/network/WorldCrypt.h>
#include <shared/network/AccountDataTypes.h>
#include <shared/dbc/ItemDbHotfixStore.h>
#include <shared/dbc/EmotesTextDbc.h>
#include <shared/dbc/LanguagesDbc.h>
#include <shared/game/ActionButton.h>
#include <shared/game/AccessLevel.h>
#include <shared/game/PhaseShift.h>
#include <shared/game/PlayerGmAppearance.h>
#include <domain/models/Character.h>
#include <domain/models/GossipMenu.h>
#include <domain/models/QuestGossip.h>
#include <domain/models/PlayerCreateInfo.h>
#include <domain/models/NpcText.h>
#include <domain/models/GmTicket.h>
#include <application/combat/CombatService.h>
#include <application/services/AuthService.h>
#include <application/services/CharacterService.h>
#include <application/world/PlayerQuestProgressStore.h>
#include <domain/repositories/IPlayerQuestProgressRepository.h>
#include <shared/network/SpellCastWire.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <array>
#include <chrono>
#include <map>
#include <optional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Firelands {

namespace gm_npc_info_ui {
struct GmNpcInfoUiSession;
}
namespace gm_ticket_ui {
struct GmTicketUiSession;
}

class SpellManager;
struct SpellCastOutcome;

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
class IQuestGossipRepository;
class IPlayerQuestProgressRepository;
class Creature;
class Map;
class Player;

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
      std::shared_ptr<LanguagesDbc const> languagesDbc,
      std::shared_ptr<ISpellDefinitionStore const> spellDefinitions,
      std::shared_ptr<IRealmRepository> realmRepo,
      std::shared_ptr<OnlineCharacterSessionRegistry> onlineCharRegistry,
      std::shared_ptr<GmTicketService> gmTicketService,
      std::shared_ptr<ItemDbHotfixStore const> itemDbHotfix,
      std::shared_ptr<SpellManager> spellManager,
      std::shared_ptr<application::CombatService> combatService,
      std::shared_ptr<INpcTemplateSearchRepository const> npcTemplateSearch,
      std::shared_ptr<FactionTemplateDbc const> factionTemplateDbc,
      std::shared_ptr<IGossipRepository> gossipRepo,
      std::shared_ptr<INpcTextRepository> npcTextRepo,
      std::shared_ptr<IQuestGossipRepository> questGossipRepo,
      std::shared_ptr<IPlayerQuestProgressRepository> questProgressRepo,
      std::shared_ptr<EmotesTextDbc const> emotesTextDbc,
      std::shared_ptr<IWorldRuntime> worldRuntime = {});

  ~WorldSession();

  IWorldRuntime &runtime() const { return *_worldRuntime; }

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
  void OnCreatureKilledByPlayer(uint64 creatureGuid, uint32 hpBefore) override;
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
  bool GmUnlearnSpell(uint32 spellId) override;
  bool GmModifyMoneyCopper(int64 deltaCopper) override;
  bool GmAddItem(uint32 itemEntry, uint32 count) override;
  bool GmRemoveItem(uint32 itemEntry, uint32 count) override;
  bool GmSetLevel(uint8 level) override;
  bool GmResetAllCooldowns() override;
  bool GmDamageUnit(uint64 targetGuid, uint32 amount) override;
  bool GmRevivePlayer(uint64 playerGuid) override;
  bool GmReviveSelf() override;

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
  void OpenGmTicketUi() override;

  bool GmNpcSearchPrintResults(std::string const &nameQuery) override;
  bool GmNpcPrintTargetInfo() override;

  PlayerGmAppearanceForUpdates GetGmAppearanceForPlayerUpdates() const {
    return _gmAppearance;
  }

  /// Per-creature combat chase / return-home state (world session combat movement).
  struct CreatureCombatRuntime {
    MovementInfo home{};
    uint32_t moveCounter = 0;
    /// In-flight `SMSG_ON_MONSTER_MOVE`: server position lerps from->to over `durationMs`.
    struct ActiveSpline {
      MovementInfo from{};
      MovementInfo to{};
      std::chrono::steady_clock::time_point startedAt{};
      uint32_t durationMs = 0;
    };
    std::optional<ActiveSpline> activeSpline;
    /// Last chase destination used for spline replanning (ref `_lastTargetPosition`).
    std::optional<MovementInfo> lastChaseTargetPos;
    std::chrono::steady_clock::time_point nextMeleeSwingAt{};
    std::chrono::steady_clock::time_point nextSpellTryAt{};
    std::vector<uint32_t> combatSpells;
    size_t nextSpellIndex = 0;
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> spellCooldownUntil;
  };

  /// After phase-related auras apply or expire (spell effects, scripts).
  void RefreshPlayerPhaseVisibilityFromAuras();
  /// Re-evaluate `phase_area` conditions after quest progress changes.
  void RefreshPlayerPhaseVisibilityFromQuestProgress();
  uint32_t ResolveSessionAreaId(uint32_t clientAreaHint) const;
  void SetSessionAreaId(uint32_t clientAreaHint);

 private:
  void ResetGmStateForLogout();
  void PublishGmVisualPatchIfInWorld();
  void PublishGmMovementPacketsIfInWorld();
  // Core Network Logic
  boost::asio::awaitable<void> ReadLoop();
  boost::asio::awaitable<void> WriteLoop();
  void ProcessInboundBuffer();
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
  void HandleGenerateRandomCharacterName(WorldPacket &packet);
  void HandlePlayerLogin(WorldPacket &packet);
  void HandleLogoutRequest(WorldPacket &packet);
  void HandleLogoutCancel(WorldPacket &packet);
  void HandleNameQuery(WorldPacket &packet);
  void HandleCreatureQuery(WorldPacket &packet);
  void HandleQueryTime(WorldPacket &packet);
  void HandlePlayedTime(WorldPacket &packet);
  void HandleMovement(WorldPacket &packet);
  void HandleMoveTeleportAck(WorldPacket &packet);
  /// Ack for server-initiated run/flight speed changes (`MovementHandler` parity).
  void HandleForceSpeedChangeAck(WorldPacket &packet);
  void HandlePing(WorldPacket &packet);
  void HandleSetSelection(WorldPacket &packet);
  void HandleTimeSyncResp(WorldPacket &packet);
  void HandleMoveTimeSkipped(WorldPacket &packet);
  void HandleMessageChat(WorldPacket &packet);
  void HandleAddonMessageChat(WorldPacket &packet);
  void HandleEmoteOpcode(WorldPacket &packet);
  void HandleTextEmoteOpcode(WorldPacket &packet);
  void HandleRealmSplit(WorldPacket &packet);
  void HandleReadyForAccountDataTimes(WorldPacket &packet);
  void HandleUpdateAccountData(WorldPacket &packet);
  void HandleRequestAccountData(WorldPacket &packet);
  void HandleGossipHello(WorldPacket &packet);
  void HandleGossipSelectOption(WorldPacket &packet);
  void HandleNpcTextQuery(WorldPacket &packet);
  void HandleListInventory(WorldPacket &packet);
  void SendVendorInventory(uint64_t vendorGuid);
  void HandleQuestGiverHello(WorldPacket &packet);
  void HandleQuestGiverQueryQuest(WorldPacket &packet);
  void HandleQuestQuery(WorldPacket &packet);
  void HandleQuestLogRemoveQuest(WorldPacket &packet);
  void HandleQuestGiverAcceptQuest(WorldPacket &packet);
  void HandleQuestGiverCompleteQuest(WorldPacket &packet);
  void HandleQuestGiverStatusQuery(WorldPacket &packet);
  void HandleTaxiNodeStatusQuery(WorldPacket &packet);
  void HandleQuestGiverStatusMultipleQuery(WorldPacket &packet);
  void HandleQueryNextMailTime(WorldPacket &packet);
  void HandleMailGetList(WorldPacket &packet);
  void HandleCalendarGetNumPending(WorldPacket &packet);
  void HandleZoneUpdate(WorldPacket &packet);
  void HandleGuildBankRemainingWithdrawMoneyQuery(WorldPacket &packet);
  void HandleLfgGetStatus(WorldPacket &packet);
  void HandleLfgLockInfoRequest(WorldPacket &packet);
  void HandleRequestCemeteryList(WorldPacket &packet);
  void HandleCastSpell(WorldPacket &packet);
  void HandleAttackSwing(WorldPacket &packet);
  void HandleAttackStop(WorldPacket &packet);
  void StopMeleeAutoAttack(bool sendStopPackets = true);
  /// Ends any in-progress melee on another target; returns false if already on `victimGuid`.
  bool PrepareMeleeRetarget(uint64_t victimGuid);
  void ScheduleMeleeAutoAttack();
  void ProcessMeleeAutoAttackTick();

  void StartCreatureAggro(uint64_t creatureGuid);
  void StopCreatureAggro(uint64_t creatureGuid, bool sendAttackStopPackets);
  void StopAllCreatureCombat(bool sendAttackStopPackets);
  void BeginCreatureReturnToHome(uint64_t creatureGuid, MovementInfo const &home,
                               uint32_t initialMoveCounter = 0);
  void ScheduleCreatureCombatMovement();
  void ProcessCreatureCombatMovementTick();
  void SyncAggroedCreatureSplinePosition(uint64_t creatureGuid);
  void ProcessCreatureCombatAttack(std::shared_ptr<Map> const &map,
                                  std::shared_ptr<Creature> const &creature,
                                  std::shared_ptr<Player> const &target,
                                   CreatureCombatRuntime &runtime);
  bool TryCastCreatureCombatSpell(std::shared_ptr<Map> const &map,
                                  std::shared_ptr<Creature> const &creature,
                                  std::shared_ptr<Player> const &target,
                                  CreatureCombatRuntime &runtime, uint32_t spellId);
  void TryAggroCreatureFromSpellDamage(uint64_t targetGuid, int32_t healthDelta);
  void FinalizeCreatureDeath(uint64 creatureGuid, uint32 hpBefore);
  void EnterPlayerCombat();
  void RefreshPlayerCombatWireState(std::shared_ptr<Map> const &map);
  bool PlayerShouldShowInCombatOnWire() const;
  void MaybeGrantKillExperience(Creature &creature, uint32 hpBefore);
  void PublishPlayerXpLevelUpdate(uint8 level, uint32 xp);
  void PublishPlayerRestStateUpdate();
  void EvadeCreatureCombat(uint64_t creatureGuid);
  bool ShouldCreatureAbandonChase(std::shared_ptr<Map> const &map,
                                  std::shared_ptr<Creature> const &creature,
                                  std::shared_ptr<Player> const &target,
                                  MovementInfo const &home) const;
  void HandleCancelAura(WorldPacket &packet);
  void HandleCancelCast(WorldPacket &packet);
  void HandleRequestCategoryCooldowns(WorldPacket &packet);

  /// Phase E: `SMSG_SPELL_COOLDOWN` (GCD + spell recovery) to the casting client.
  void SendClientSpellCooldownsAfterCast(
      uint32 spellId, uint32 spellCooldownDurationMs,
      std::chrono::steady_clock::time_point gcdReady,
      std::chrono::steady_clock::time_point now);
  /// All active per-spell recovery timers (`SMSG_SPELL_COOLDOWN`, login / relog).
  void SendClientActiveSpellCooldowns();
  /// Active shared category timers (`SMSG_CATEGORY_COOLDOWN`).
  void SendClientActiveCategoryCooldowns();
  /// Updates session CD maps and pushes spell + category cooldown packets to the client.
  void CommitSpellCooldownsFromCast(uint32 spellId, SpellCastOutcome const &out,
      std::chrono::steady_clock::time_point now);
  /// Deferred `SMSG_SPELL_GO` completion: spell recovery only (GCD was sent at cast start).
  void CommitSpellRecoveryCooldownFromDeferred(uint32 spellId,
                                               uint32 spellCooldownDurationMs,
      std::chrono::steady_clock::time_point now);
  void RestorePersistedSpellCooldowns(uint32 characterGuid);
  void SavePersistedSpellCooldowns(uint32 characterGuid);
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
    /// Character-select / pre-player socket parity (`SendTutorialsData` without player).
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
    /// Same payload as `Player::LearnSpell` → `SMSG_LEARNED_SPELL`.
  void SendLearnedSpell(uint32 spellId);
  void SendUnlearnSpellsEmpty();
  void SendUnlearnSpells(std::vector<uint32> const &spellIds);
  void SendDungeonDifficulty(bool inGroup = false);
  void SendHotfixNotifyBlobEmpty();
  void SendContactListEmpty();
  void SendAllAchievementDataEmpty();
  void SendEquipmentSetListEmpty();
  void SendActionButtons(uint8_t reason);
  void SendInitialActionButtons();
  void HandleSetActionButton(WorldPacket &packet);
  void HandleSetActionBarToggles(WorldPacket &packet);
  void HandleLoadingScreenNotify(WorldPacket &packet);
  void HandleObjectUpdateFailed(WorldPacket &packet);
  void LoadActionButtonsForCharacter(uint32_t characterGuid);
  void SaveActionButtonsForCharacter(uint32_t characterGuid);
  void SendActionBarTogglesUpdate();
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
                         std::vector<GossipMenuItem> const &items,
                         std::vector<GossipQuestItem> const &quests = {});
  void SendGossipComplete();
  /// Push `SMSG_NPC_TEXT_UPDATE` for gossip window body (client often skips query on 4.3.4).
  void SendNpcTextForGossipWindow(uint32_t textId);
  std::optional<uint32_t> TryResolveCreatureTemplateEntry(uint64_t npcGuid) const;
  bool TrySendDatabaseGossipMenu(uint64_t npcGuid, uint32_t templateEntry);
  /// Gossip menu or quest list for a quest giver NPC (used by gossip + quest hello opcodes).
  bool TryOpenQuestGiverDialog(uint64_t npcGuid);
  struct CreatureClientWireFields {
    uint32 displayId = 0;
    uint32 npcFlags = 0;
    uint32 unitFieldFlags = 0;
    uint32 unitFieldFlags2 = 0;
  };
  bool GmSeesAllCreatures() const { return _gmAppearance.gmTagOn; }
  uint32_t ResolveEffectiveNpcFlagsForCreature(Creature const &creature) const;
  CreatureClientWireFields ResolveCreatureWireFieldsForClient(
      Creature const &creature) const;
  void SendQuestGiverStatusForGuid(uint64_t npcGuid, uint32_t creatureEntry);
  void SendQuestGiverStatusMultipleNearby();
  /// Marks meet-NPC quests complete when the player opens the turn-in creature.
  void TryProgressMeetQuestsAtCreature(uint32_t creatureEntry);
  /// Adds quest to log and sends `PLAYER_QUEST_LOG_*` update (ref `AddQuest` / `SetQuestSlot`).
  bool TryGrantQuestFromGiver(uint64_t npcGuid, uint32_t creatureEntry,
                            QuestGossipSummary const &summary);

  /// `SMSG_UPDATE_OBJECT` for one `PLAYER_QUEST_LOG_*` slot (ref `SetQuestSlot`).
  bool SendPlayerQuestLogSlotWire(uint32_t questId);

  void SendGmTicketMainMenu();
  void SendGmTicketListMenu();
  void SendGmTicketDetailMenu();
  void NotifyPlayerGmTicketReply(GmTicket const &ticket);
  bool TryBuildGmTicketNpcText(uint32_t textId, NpcText &out) const;
  bool TryHandleGmTicketGossipSelect(uint64_t npcGuid, uint32_t menuId,
                                     uint32_t listId, std::string const &code);
  std::unique_ptr<gm_ticket_ui::GmTicketUiSession> _gmTicketUi;

  bool OpenGmNpcInfoGossip();
  void SendGmNpcInfoMainMenu();
  void SendGmNpcInfoSubMenu(uint32_t menuId, uint32_t textId);
  bool TryBuildGmNpcInfoNpcText(uint32_t textId, NpcText &out) const;
  bool TryHandleGmNpcInfoGossipSelect(uint64_t npcGuid, uint32_t menuId,
                                      uint32_t listId, std::string const &code);
  std::unique_ptr<gm_npc_info_ui::GmNpcInfoUiSession> _gmNpcInfoUi;

  // Helpers
    /// schedules the next time-sync ~5s after SendTimeSync; never chains on
  /// every CMSG_TIME_SYNC_RESP (that floods the client and breaks map loading).
  void SchedulePeriodicTimeSync();
  void CancelPeriodicTimeSync();
  boost::asio::awaitable<void> TimeSyncLoop();
  void ResetBreathMirrorState();
  void UpdateBreathFromSwimmingState(bool swimming);
  /// Pushes `UNIT_FIELD_BYTES_1` anim tier when swim/fly state changes (login uses create).
  /// `inLiquidForBreath` matches breath logic (swim opcodes can lead movement flags).
  void SyncPlayerMovementHintsIfNeeded(bool inLiquidForBreath = false);
  void SendStartMirrorTimerPacket(int32_t timerType, int32_t value, int32_t maxValue,
                                  int32_t scale, bool paused, int32_t spellId);
  void SendStopMirrorTimerPacket(int32_t timerType);

  /// CMSG_PLAYER_LOGIN sub-steps (keeps `HandlePlayerLogin` readable).
  void LoginSendAccountDataAndPreMapPackets(uint64 guid, Character const &character);
  void LoginBuildKnownSpellsAndSendSpellbook(Character const &character);
  void RefreshKnownSpellsForCharacter(Character const &character);
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
  void RebuildPlayerPhaseShiftFromActiveAuras();
  void SendPlayerPhaseShiftToClient();
  void RefreshNearbyCreaturePhaseVisibility(float x, float y);
  void RefreshNearbyCreatureGmWireFlags();
  bool IsCreatureVisibleToPlayer(Creature const &creature) const;
  void LoginFinalizeWorldEntry(uint64 guid);
  void LoadQuestProgressForCharacter(uint32 characterGuid);
  void PersistQuestProgressForCharacter();
  void SendRestoredQuestLogToClient();
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
    bool hasAuraApply = false;
    uint64 auraTargetGuid = 0;
    uint64 auraCasterGuid = 0;
    uint32 auraSpellId = 0;
    uint32 auraEffectType = 0;
    uint8 auraEffectIndex = 0;
    int32 auraBasePoints = 0;
    int32 auraDieSides = 0;
    uint32 auraDurationMs = 0;
    uint32 auraPeriodicPeriodMs = 0;
    int32 auraPeriodicHealthDeltaPerTick = 0;
    bool auraIsNegative = false;
    uint8 auraCasterLevel = 1;
    bool spellGoMissile = false;
    SpellCastWire::SpellMissileTrajectoryWire missile{};
    uint32 spellImpactDelayMs = 0;
  };

  void CancelPendingClientSpellCast();
  void ScheduleDeferredSpellCastCompletion(SpellCastOutcome const &out);
  void CompleteDeferredSpellCast(PendingSpellCastFinish const &finish);
  void ScheduleSpellImpactVisual(std::shared_ptr<Map> map, uint64 casterGuid, uint32 spellId,
                                 uint64 hitTargetGuid, uint32 delayMs);

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
  std::shared_ptr<application::CombatService> _combatService;
  std::shared_ptr<INpcTemplateSearchRepository const> _npcTemplateSearch;
  std::shared_ptr<FactionTemplateDbc const> _factionTemplateDbc;
  std::shared_ptr<IGossipRepository> _gossipRepo;
  std::shared_ptr<INpcTextRepository> _npcTextRepo;
  std::shared_ptr<IQuestGossipRepository> _questGossipRepo;
  std::shared_ptr<IPlayerQuestProgressRepository> _questProgressRepo;
  std::shared_ptr<EmotesTextDbc const> _emotesTextDbc;
  std::shared_ptr<IWorldRuntime> _worldRuntime;
  PlayerQuestProgressStore _questProgress;

  bool IsActivePlayerAlive() const;
  void ApplyUnitNpcEmoteState(uint32_t emoteAnim);
  void BroadcastEmoteAnimation(uint32_t emoteAnim);
  void BroadcastTextEmote(uint32_t textEmote, uint32_t emoteNum,
                          uint64_t targetGuid);
  void TryClearEmotesOnMovement(WorldOpcode opcode, bool positionChanged = false);
  std::string ResolveTextEmoteTargetName(uint64_t targetGuid) const;
  uint32_t _unitNpcEmoteState = 0;

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
  uint8 _playerClass = 0;
  uint8 _playerLevel = 1;
    /// Login combat snapshot for casts when the live map `Player` is not found yet.
    uint32 _loginPower1 = 0;
    uint32 _loginMaxPower1 = 0;
  /// Persisted copper; mirrored on logout and after `.money` GM commands.
  uint32_t _moneyCopper = 0;
  /// Persisted experience (`characters.xp`); mirrored on logout and GM level.
  uint32_t _playerXp = 0;
  /// Rested XP pool (`characters.rest_bonus` / `PLAYER_REST_STATE_EXPERIENCE`).
  float _playerRestBonus = 0.f;
  uint8 _playerFacialHair = 0;
  bool _sentOpeningCinematic = false;
  std::array<uint32_t, Character::kTutorialMaskInts> _tutorialInts{};
  uint32 _mapId = 0;
  uint32 _zoneId = 0;
  /// `AreaTable.dbc` id for `phase_area` (`CMSG_ZONEUPDATE` area field / spawn).
  uint32 _areaId = 0;
  MovementInfo _position;
  uint8 _readBuffer[2048];
  ByteBuffer _inBuffer;
  WorldCrypt _crypt;
  uint8 _decHeader[6]{};
  bool _headerDecrypted = false;

  // Write queue: drained by WriteLoop (single serialized co_await AsyncWrite chain)
  std::deque<std::shared_ptr<std::vector<uint8>>> _writeQueue;
  std::mutex _writeMutex;
  boost::asio::steady_timer _writeWakeTimer;

  // Diagnostics: last SMSG sent (helps correlate client disconnect/crash)
  uint32 _lastSentOpcode = 0;
  uint32 _lastSentPayloadSize = 0;

  /// Monotonic counter for SMSG_TIME_SYNC_REQ (see WorldSession::SendTimeSync in reference).
  uint32 _timeSyncNextCounter = 0;

  boost::asio::steady_timer _timeSyncPeriodicTimer;
  boost::asio::steady_timer _pendingSpellCastTimer;
  boost::asio::steady_timer _meleeAutoAttackTimer;
  boost::asio::steady_timer _creatureCombatMoveTimer;

  /// Hostile creatures actively chasing this player (multiple allowed).
  std::unordered_map<uint64_t, CreatureCombatRuntime> _creatureAggroed;
  /// Creatures walking back to `home` after dropping aggro.
  std::unordered_map<uint64_t, CreatureCombatRuntime> _creatureReturningHome;

  uint64_t _meleeVictimGuid = 0;
  bool _meleeVictimIsCreature = false;
  static constexpr uint32_t kDefaultMainhandSwingMs = 2000u;
  bool _pendingDeferredCastActive = false;
  uint8 _pendingCastId = 0;
  uint32 _pendingSpellId = 0;
  std::atomic<bool> _periodicTimeSyncRunning{false};

  /// Filled while handling CMSG_AUTH_SESSION; consumed by SendAddonInfo (SMSG_ADDON_INFO).
  std::vector<AuthSecureAddonEntry> _authSecureAddons;

  /// Known spells for the logged-in character (mirrors `SMSG_SEND_KNOWN_SPELLS` payload).
  std::vector<uint32> _knownSpells;
  /// Same ids as `_knownSpells` for O(1) lookups (`SpellManager`, GM helpers).
  std::unordered_set<uint32> _knownSpellIds;
  std::vector<StarterSkillGrant> _knownSkills;
  std::chrono::steady_clock::time_point _gcdReady{};
  /// Spell id from the cast that started the current GCD (client clear packet).
  uint32 _gcdTriggerSpellId = 0;
  /// Phase E: per-spell recovery (`SpellCooldowns.dbc` RecoveryTime) until instant.
  std::unordered_map<uint32, std::chrono::steady_clock::time_point> _spellCooldownUntil;
  /// Phase E: shared category recovery (`SpellCooldowns` + `SpellCategories.dbc` group).
  std::unordered_map<uint32, std::chrono::steady_clock::time_point>
      _spellCategoryCooldownUntil;

  std::array<ActionButton::PackedActionBar, ActionButton::kMaxActionBarSpecs>
      _actionButtonBySpec{};
  uint8_t _activeActionBarSpec = 0;
  uint8_t _actionBarToggles = 0xFF;

  ActionButton::PackedActionBar &ActiveActionBar() {
    return _actionButtonBySpec[std::min<size_t>(_activeActionBarSpec,
                                                ActionButton::kMaxActionBarSpecs - 1)];
  }
  ActionButton::PackedActionBar const &ActiveActionBar() const {
    return _actionButtonBySpec[std::min<size_t>(_activeActionBarSpec,
                                                ActionButton::kMaxActionBarSpecs - 1)];
  }

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
  std::optional<uint8_t> _movementAnimTierSent;

  /// Faction.dbc id → forced `ReputationRank` for `SMSG_SET_FORCED_REACTIONS` (quest/script overrides).
  std::map<uint32, uint32> _forcedFactionReactions;

  /// Set when this session sends `SMSG_GOSSIP_MESSAGE` (Lua may add APIs later).
  bool _gossipMenuSent = false;

  PhaseShift _playerPhaseShift;
  std::unordered_set<uint64> _visibleCreatureGuids;
  };

} // namespace Firelands

#endif
