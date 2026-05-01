#include <cstring>
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

void RealmLinkSession::Start() { DoRead(); }

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

bool RealmLinkSession::tryConsumeHandshake() {
  if (_handshakeDone)
    return true;

  if (_rx.size() < 6)
    return false;

  uint32_t const magic = readU32Le(_rx, 0);
  uint16_t const tokenLen = readU16Le(_rx, 4);
  if (tokenLen > kRealmLinkMaxTokenLen) {
    FailHandshake(kRealmLinkAckReject);
    return true;
  }

  size_t const need = 6u + static_cast<size_t>(tokenLen) + 4u;
  if (_rx.size() < need)
    return false;

  if (magic != kRealmLinkMagic) {
    FailHandshake(kRealmLinkAckReject);
    return true;
  }

  std::string token(reinterpret_cast<char const *>(_rx.data() + 6),
                      static_cast<size_t>(tokenLen));
  uint32_t const realmId = readU32Le(_rx, 6 + tokenLen);

  if (!ConstantTimeTokenEquals(token, _expectedToken)) {
    LOG_WARN("Realm-link: bad token for realm id {}", realmId);
    FailHandshake(kRealmLinkAckReject);
    return true;
  }

  if (!_registry) {
    FailHandshake(kRealmLinkAckReject);
    return true;
  }

  if (_registry->tryClaim(realmId) != RealmLiveRegistry::ClaimResult::Ok) {
    LOG_WARN("Realm-link: realm {} already has an active world connection",
             realmId);
    FailHandshake(kRealmLinkAckReject);
    return true;
  }

  _registered = true;
  _realmId = realmId;
  SendAck(kRealmLinkAckOk);
  _handshakeDone = true;
  _rx.erase(_rx.begin(), _rx.begin() + static_cast<std::ptrdiff_t>(need));
  LOG_INFO("Realm-link: world registered for realm {}", realmId);
  return true;
}

void RealmLinkSession::SendAck(uint8_t value) {
  auto self = shared_from_this();
  auto buf = std::make_shared<std::vector<uint8>>();
  buf->push_back(value);
  boost::asio::async_write(
      _socket, boost::asio::buffer(buf->data(), buf->size()),
      [self, buf](boost::system::error_code /*ec*/, std::size_t /*n*/) {});
}

void RealmLinkSession::FailHandshake(uint8_t ack) {
  SendAck(ack);
  _socket.close();
}

void RealmLinkSession::OnDisconnect() {
  if (_registered && _registry) {
    _registry->release(_realmId);
    LOG_INFO("Realm-link: world disconnected for realm {}", _realmId);
    _registered = false;
  }
}

void RealmLinkSession::DoRead() {
  auto self = shared_from_this();
  _socket.async_read_some(
      boost::asio::buffer(_readBuf, sizeof(_readBuf)),
      [this, self](boost::system::error_code ec, std::size_t n) {
        if (ec) {
          OnDisconnect();
          return;
        }
        _rx.insert(_rx.end(), _readBuf, _readBuf + n);
        if (!_handshakeDone) {
          if (!tryConsumeHandshake()) {
            DoRead();
            return;
          }
          if (!_handshakeDone) // failed and closed
            return;
        } else {
          // Discard pings / extra bytes; connection liveness is the signal.
          _rx.clear();
        }
        DoRead();
      });
}

} // namespace Firelands
