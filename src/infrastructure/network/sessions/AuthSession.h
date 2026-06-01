#ifndef FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_AUTH_SESSION_H
#define FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_AUTH_SESSION_H

#include <application/ports/IAuthSession.h>
#include <application/services/AuthService.h>
#include <application/services/RealmListService.h>
#include <application/services/SRPService.h>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <deque>
#include <memory>
#include <mutex>
#include <shared/game/Permissions.h>
#include <shared/network/AuthPacket.h>
#include <shared/network/AuthPackets.h>

namespace Firelands {

using boost::asio::ip::tcp;

class IRbacRepository;

class AuthSession : public IAuthSession,
                    public std::enable_shared_from_this<AuthSession> {
public:
  AuthSession(tcp::socket socket, std::shared_ptr<AuthService> authService,
              std::shared_ptr<RealmListService> realmService,
              std::shared_ptr<IRbacRepository> rbacRepo = {});

  void Start();

  // IAuthSession implementation
  void SendPacket(AuthPacket &packet);
  void SendPacket(ByteBuffer &buffer) override;
  void Close() override;
  std::string GetIpAddress() const override;

private:
  boost::asio::awaitable<void> ReadLoop();
  boost::asio::awaitable<void> WriteLoop();
  void QueueOutgoing(std::shared_ptr<std::vector<uint8>> buffer);
  void HandlePacket(ByteBuffer &buffer);
  void ProcessPacket(AuthPacket &packet);

  void HandleLogonChallenge(AuthPacket &packet);
  void HandleLogonProof(AuthPacket &packet);
  void HandleRealmList(AuthPacket &packet);

  tcp::socket _socket;
  std::shared_ptr<AuthService> _authService;
  std::shared_ptr<RealmListService> _realmService;

  uint8 _readBuffer[1024];

  std::deque<std::shared_ptr<std::vector<uint8>>> _writeQueue;
  std::mutex _writeMutex;
  boost::asio::steady_timer _writeWakeTimer;

  // SRP Session State
  std::string _username;
  std::vector<uint8> _salt;
  std::unique_ptr<BigInt> _v;
  std::unique_ptr<BigInt> _b;
  std::unique_ptr<BigInt> _B;

  std::shared_ptr<IRbacRepository> _rbacRepo;
  /// Loaded after successful `AUTH_LOGON_PROOF` for realm list filtering.
  PermissionMask _accountRolePermissionMask = 0;
};

} // namespace Firelands

#endif // FIRELANDS_INFRASTRUCTURE_NETWORK_SESSIONS_AUTH_SESSION_H
