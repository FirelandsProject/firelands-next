#ifndef FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_WORLD_SESSION_H
#define FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_WORLD_SESSION_H

#include <application/ports/IAuthSession.h>
#include <application/services/AuthService.h>
#include <application/services/CharacterService.h>
#include <shared/network/BitWriter.h>
#include <shared/network/ByteBuffer.h>
#include <shared/network/WorldPacket.h>
#include <shared/network/WorldOpcodes.h>
#include <boost/asio.hpp>
#include <memory>
#include <string>

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
        void HandleCharEnum(WorldPacket& packet);
        void HandleCharCreate(WorldPacket& packet);
        void HandleCharDelete(WorldPacket& packet);
        void HandlePlayerLogin(WorldPacket& packet);
        void SendInitialObjectUpdate(uint64 guid);
        void WritePackedGuid(uint64 guid, BitWriter& bw, ByteBuffer& bb);

        tcp::socket _socket;
        std::shared_ptr<AuthService> _authService;
        std::shared_ptr<CharacterService> _charService;
        uint32 _serverSeed;
        uint32 _accountId;
        uint8 _readBuffer[2048];
    };

} // namespace Firelands

#endif
