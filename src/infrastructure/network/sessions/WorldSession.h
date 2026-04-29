#ifndef FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_WORLD_SESSION_H
#define FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_WORLD_SESSION_H

#include <application/ports/IAuthSession.h>
#include <application/ports/ICommandService.h>
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
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <string>
#include <vector>

namespace Firelands {

using boost::asio::ip::tcp;

class WorldSession : public IAuthSession,
                     public IMapNotifier,
                     public std::enable_shared_from_this<WorldSession> {
public:
  explicit WorldSession(tcp::socket socket,
                        std::shared_ptr<AuthService> authService,
                        std::shared_ptr<CharacterService> charService,
                        std::shared_ptr<ICommandService> commandService);

  void Start();

  void SendPacket(WorldPacket &packet) override;
  void SendPacket(ServerPacket *packet);
  void SendPacket(ByteBuffer &buffer) override;
  void SendAuthChallenge();
  void Close() override;
  std::string GetIpAddress() const override;

  // Command Support
  void SendNotification(const std::string &message);
  void TeleportTo(uint32 mapId, float x, float y, float z,
                  float orientation = 0.0f);

  uint64 GetGuid() const override { return _playerGuid; }
  const MovementInfo &GetPosition() const { return _position; }

private:
private:
  // Core Network Logic
  void DoRead();
  void DoWrite();
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
  void HandleNameQuery(WorldPacket &packet);
  void HandleQueryTime(WorldPacket &packet);
  void HandlePlayedTime(WorldPacket &packet);
  void HandleMovement(WorldPacket &packet);
  void HandlePing(WorldPacket &packet);
  void HandleTimeSyncResp(WorldPacket &packet);
  void HandleMoveTimeSkipped(WorldPacket &packet);
  void HandleMessageChat(WorldPacket &packet);
  void HandleRealmSplit(WorldPacket &packet);
  void HandleReadyForAccountDataTimes(WorldPacket &packet);
  void HandleUpdateAccountData(WorldPacket &packet);
  void HandleGossipHello(WorldPacket &packet);
  void HandleGossipSelectOption(WorldPacket &packet);
  void HandleQueryNextMailTime(WorldPacket &packet);
  void HandleCalendarGetNumPending(WorldPacket &packet);
  void HandleZoneUpdate(WorldPacket &packet);

  // Server Packet Senders (SMSG)
  void SendAuthResponse();
  void SendAddonInfo();
  void SendClientCacheVersion(uint32 version = 0);
  void SendTutorialFlags();
  void SendAccountDataTimes(uint32 mask);
  void SendFeatureSystemStatus();
  void SendRealmSplit(uint32 realmId);
  void SendLoginSetTimeSpeed(float speed = 0.01666667f);
  void SendLearnedDanceMoves();
  void SendMotd();
  void SendInitialObjectUpdate(uint64 guid);
  /// Matches WorldPackets::Spells::SendKnownSpells (FirelandsCore / TCPP 4.3.4).
  void SendKnownSpells(bool initialLogin, std::vector<uint32> const &spellIds);
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

  // Helpers
  void ReadMovementInfo(WorldPacket &packet, MovementInfo &move);

  tcp::socket _socket;
  std::shared_ptr<AuthService> _authService;
  std::shared_ptr<CharacterService> _charService;
  std::shared_ptr<ICommandService> _commandService;
  bool _initialized = false;
  uint32 _serverSeed;
  uint32 _accountId = 0;
  uint64 _playerGuid = 0;
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
};

} // namespace Firelands

#endif
