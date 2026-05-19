#pragma once

#include <infrastructure/network/realm_link/RealmLiveRegistry.h>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Firelands {

using boost::asio::ip::tcp;

/// Inbound TCP session from worldserver (handshake + optional ping bytes).
class RealmLinkSession : public std::enable_shared_from_this<RealmLinkSession> {
public:
  RealmLinkSession(tcp::socket socket,
                   std::shared_ptr<RealmLiveRegistry> registry,
                   std::string expectedToken);

  void Start();

private:
  enum class HandshakeConsume { NeedMore, Accepted, Rejected };

  boost::asio::awaitable<void> ReadLoop();
  boost::asio::awaitable<void> SendAck(uint8_t value);
  HandshakeConsume tryConsumeHandshake();
  void OnDisconnect();

  static uint32_t readU32Le(std::vector<uint8> const &b, size_t pos);
  static uint16_t readU16Le(std::vector<uint8> const &b, size_t pos);

  tcp::socket _socket;
  std::shared_ptr<RealmLiveRegistry> _registry;
  std::string _expectedToken;

  std::vector<uint8> _rx;
  bool _handshakeDone = false;
  bool _registered = false;
  uint32_t _realmId = 0;
  uint8_t _readBuf[512];
};

} // namespace Firelands
