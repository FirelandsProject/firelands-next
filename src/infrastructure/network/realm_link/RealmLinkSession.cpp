#include <cstring>
#include <infrastructure/network/asio/AsioAwaitables.h>
#include <infrastructure/network/realm_link/RealmLinkSession.h>
#include <shared/Logger.h>
#include <shared/network/RealmLinkProtocol.h>

namespace Firelands {

namespace {

bool ConstantTimeTokenEquals(std::string const &a, std::string const &b) {
  if (a.size() != b.size())
    return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < a.size(); ++i)
    diff |= static_cast<uint8_t>(a[i] ^ b[i]);
  return diff == 0;
}

} // namespace

RealmLinkSession::RealmLinkSession(tcp::socket socket,
                                   std::shared_ptr<RealmLiveRegistry> registry,
                                   std::string expectedToken)
    : _socket(std::move(socket)), _registry(std::move(registry)),
      _expectedToken(std::move(expectedToken)) {}

void RealmLinkSession::Start() {
  auto self = shared_from_this();
  Asio::SpawnDetached(_socket.get_executor(),
                      [self, this]() -> Asio::awaitable<void> {
                        co_await ReadLoop();
                      });
}

uint32_t RealmLinkSession::readU32Le(std::vector<uint8> const &b, size_t pos) {
  uint32_t v = 0;
  if (pos + 4 <= b.size())
    std::memcpy(&v, b.data() + pos, 4);
  return v;
}

uint16_t RealmLinkSession::readU16Le(std::vector<uint8> const &b, size_t pos) {
  uint16_t v = 0;
  if (pos + 2 <= b.size())
    std::memcpy(&v, b.data() + pos, 2);
  return v;
}

RealmLinkSession::HandshakeConsume RealmLinkSession::tryConsumeHandshake() {
  if (_handshakeDone)
    return HandshakeConsume::Accepted;

  if (_rx.size() < 6)
    return HandshakeConsume::NeedMore;

  uint32_t const magic = readU32Le(_rx, 0);
  uint16_t const tokenLen = readU16Le(_rx, 4);
  if (tokenLen > kRealmLinkMaxTokenLen)
    return HandshakeConsume::Rejected;

  size_t const need = 6u + static_cast<size_t>(tokenLen) + 4u;
  if (_rx.size() < need)
    return HandshakeConsume::NeedMore;

  if (magic != kRealmLinkMagic)
    return HandshakeConsume::Rejected;

  std::string token(reinterpret_cast<char const *>(_rx.data() + 6),
                    static_cast<size_t>(tokenLen));
  uint32_t const realmId = readU32Le(_rx, 6 + tokenLen);

  if (!ConstantTimeTokenEquals(token, _expectedToken)) {
    LOG_WARN("Realm-link: bad token for realm id {}", realmId);
    return HandshakeConsume::Rejected;
  }

  if (!_registry)
    return HandshakeConsume::Rejected;

  if (_registry->tryClaim(realmId) != RealmLiveRegistry::ClaimResult::Ok) {
    LOG_WARN("Realm-link: realm {} already has an active world connection",
             realmId);
    return HandshakeConsume::Rejected;
  }

  _registered = true;
  _realmId = realmId;
  _handshakeDone = true;
  _rx.erase(_rx.begin(), _rx.begin() + static_cast<std::ptrdiff_t>(need));
  LOG_DEBUG("Realm-link: world registered for realm {}", realmId);
  return HandshakeConsume::Accepted;
}

Asio::awaitable<void> RealmLinkSession::SendAck(uint8_t value) {
  uint8_t byte = value;
  co_await Asio::AsyncWrite(_socket, std::span<const uint8>(&byte, 1));
}

void RealmLinkSession::OnDisconnect() {
  if (_registered && _registry) {
    _registry->release(_realmId);
    LOG_DEBUG("Realm-link: world disconnected for realm {}", _realmId);
    _registered = false;
  }
}

Asio::awaitable<void> RealmLinkSession::ReadLoop() {
  const std::span<uint8_t> readSpan(_readBuf, sizeof(_readBuf));

  try {
    while (_socket.is_open()) {
      std::size_t const n = co_await Asio::AsyncReadSome(_socket, readSpan);
      if (n == 0)
        continue;

      _rx.insert(_rx.end(), _readBuf, _readBuf + n);

      if (!_handshakeDone) {
        switch (tryConsumeHandshake()) {
        case HandshakeConsume::NeedMore:
          continue;
        case HandshakeConsume::Rejected:
          co_await SendAck(kRealmLinkAckReject);
          _socket.close();
          OnDisconnect();
          co_return;
        case HandshakeConsume::Accepted:
          co_await SendAck(kRealmLinkAckOk);
          break;
        }
      } else {
        // Discard pings / extra bytes; connection liveness is the signal.
        _rx.clear();
      }
    }
  } catch (const boost::system::system_error &e) {
    if (e.code() != boost::asio::error::operation_aborted &&
        e.code() != boost::asio::error::eof) {
      LOG_DEBUG("Realm-link read ended: {}", e.what());
    }
    OnDisconnect();
  }
}

} // namespace Firelands
