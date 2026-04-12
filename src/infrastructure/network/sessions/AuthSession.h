#ifndef FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_AUTH_SESSION_H
#define FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_AUTH_SESSION_H

#include <application/ports/IAuthSession.h>
#include <application/services/AuthService.h>
#include <application/services/SRPService.h>
#include <shared/network/AuthPackets.h>
#include <boost/asio.hpp>
#include <memory>
#include <queue>

namespace Firelands {

    using boost::asio::ip::tcp;

    class AuthSession : public IAuthSession, public std::enable_shared_from_this<AuthSession> {
    public:
        AuthSession(tcp::socket socket, std::shared_ptr<AuthService> authService);
        
        void Start();
        
        // IAuthSession implementation
        void SendPacket(ByteBuffer& buffer) override;
        void Close() override;
        std::string GetIpAddress() const override;

    private:
        void DoRead();
        void HandlePacket(ByteBuffer& buffer);
        void HandleLogonChallenge(ByteBuffer& buffer);
        void HandleLogonProof(ByteBuffer& buffer);
        void DoWrite();

        tcp::socket _socket;
        std::shared_ptr<AuthService> _authService;
        
        uint8 _readBuffer[1024];
        
        // SRP Session State
        std::string _username;
        std::vector<uint8> _salt;
        std::unique_ptr<BigInt> _v;
        std::unique_ptr<BigInt> _b;
        std::unique_ptr<BigInt> _B;
    };

} // namespace Firelands

#endif // FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_AUTH_SESSION_H
