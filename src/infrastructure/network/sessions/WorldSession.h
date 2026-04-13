#ifndef FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_WORLD_SESSION_H
#define FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_WORLD_SESSION_H

#include <application/ports/IAuthSession.h>
#include <application/services/AuthService.h>
#include <application/services/CharacterService.h>
#include <shared/network/BitWriter.h>
#include <shared/network/ByteBuffer.h>
#include <shared/network/WorldPacket.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/MovementInfo.h>
#include <shared/network/WorldCrypt.h>
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <deque>
#include <mutex>

namespace Firelands {

    using boost::asio::ip::tcp;

    class WorldSession : public IAuthSession, public std::enable_shared_from_this<WorldSession> {
    public:
        explicit WorldSession(tcp::socket socket, std::shared_ptr<AuthService> authService, std::shared_ptr<CharacterService> charService);
        
        void Start();
        
        void SendPacket(WorldPacket& packet);
        void SendPacket(ByteBuffer& buffer) override;
        void SendAuthChallenge();
        void Close() override;
        std::string GetIpAddress() const override;

    private:
        void DoRead();
        void HandlePacket(ByteBuffer& buffer);
        void ProcessPacket(WorldPacket& packet);
        
        void HandleAuthSession(WorldPacket& packet);
        void HandlePing(WorldPacket& packet);
        void HandleCharEnum(WorldPacket& packet);
        void HandleCharCreate(WorldPacket& packet);
        void HandleCharDelete(WorldPacket& packet);
        void HandlePlayerLogin(WorldPacket& packet);
        void HandleMovement(WorldPacket& packet);
        void HandleMessageChat(WorldPacket& packet);
        void HandleReadyForAccountDataTimes(WorldPacket& packet);
        void SendInitialObjectUpdate(uint64 guid);
        
        void ReadMovementInfo(WorldPacket& packet, MovementInfo& move);

        // Auth Refactor
        void HandleAuthSessionScattered(WorldPacket& packet, uint8* digest, std::vector<uint8>& localChallenge, uint16& build, uint32& realmId, int32& loginServerId);
        void HandleAuthSessionStandard(WorldPacket& packet, uint16& build, uint8* digest, std::vector<uint8>& localChallenge, uint32& realmId);
        
        void SendAuthResponse();
        void SendAddonInfo();
        void SendClientCacheVersion();
        void SendTutorialFlags();
        void SendAccountDataTimes();
        void SendFeatureSystemStatus();
        void SendRealmSplit(uint32 realmId);
        void SendLoginSetTimeSpeed();
        void SendSetTimeZoneInformation();
        void SendLearnedDanceMoves();
        void SendMotd();
        void SendAccountRestrictedUpdate();
        void SendSetDfFastLaunchResources();
        void SendInitialRaidGroupError();

        void DoWrite();

        tcp::socket _socket;
        std::shared_ptr<AuthService> _authService;
        std::shared_ptr<CharacterService> _charService;
        uint32 _serverSeed;
        uint32 _accountId;
        uint64 _playerGuid = 0;
        MovementInfo _position;
        bool _initialized = false;
        uint8 _readBuffer[2048];
        ByteBuffer _inBuffer;
        WorldCrypt _crypt;
        uint8 _decHeader[6]{};
        bool _headerDecrypted = false;

        // Write queue: serializes async_write calls to prevent interleaving
        std::deque<std::shared_ptr<std::vector<uint8>>> _writeQueue;
        bool _writing = false;
    };

} // namespace Firelands

#endif
