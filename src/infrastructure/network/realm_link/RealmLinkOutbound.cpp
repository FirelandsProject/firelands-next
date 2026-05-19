#include <infrastructure/network/realm_link/RealmLinkOutbound.h>
#include <infrastructure/network/asio/AsioAwaitables.h>
#include <shared/Config.h>
#include <shared/Logger.h>
#include <shared/network/RealmLinkProtocol.h>

#include <algorithm>
#include <boost/asio.hpp>
#include <boost/asio/redirect_error.hpp>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#endif

namespace Firelands {

namespace {

void appendU32Le(std::vector<uint8> &out, uint32_t v) {
  out.push_back(static_cast<uint8>(v & 0xFF));
  out.push_back(static_cast<uint8>((v >> 8) & 0xFF));
  out.push_back(static_cast<uint8>((v >> 16) & 0xFF));
  out.push_back(static_cast<uint8>((v >> 24) & 0xFF));
}

void appendU16Le(std::vector<uint8> &out, uint16_t v) {
  out.push_back(static_cast<uint8>(v & 0xFF));
  out.push_back(static_cast<uint8>((v >> 8) & 0xFF));
}

void applyTcpIoTimeouts(boost::asio::ip::tcp::socket &socket) {
#if defined(_WIN32)
  DWORD const ms = 5000;
  SOCKET const h = socket.native_handle();
  if (h != INVALID_SOCKET) {
    (void)::setsockopt(h, SOL_SOCKET, SO_RCVTIMEO,
                       reinterpret_cast<char const *>(&ms), sizeof(ms));
    (void)::setsockopt(h, SOL_SOCKET, SO_SNDTIMEO,
                       reinterpret_cast<char const *>(&ms), sizeof(ms));
  }
#else
  timeval tv{};
  tv.tv_sec = 5;
  tv.tv_usec = 0;
  int const fd = socket.native_handle();
  if (fd >= 0) {
    (void)::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    (void)::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  }
#endif
}

using boost::asio::ip::tcp;

Asio::awaitable<void> OutboundLinkSession(tcp::socket &socket,
                                          std::atomic<bool> const &stop,
                                          std::vector<uint8> const &handshake,
                                          std::string const &host, uint16_t port,
                                          uint32_t realmId) {
  co_await Asio::AsyncWrite(
      socket, std::span<const uint8>(handshake.data(), handshake.size()));

  uint8_t ack = kRealmLinkAckReject;
  co_await boost::asio::async_read(
      socket, boost::asio::buffer(&ack, 1), Asio::use_awaitable);

  if (ack != kRealmLinkAckOk) {
    throw std::runtime_error(
        "auth rejected handshake (ack=" + std::to_string(static_cast<int>(ack)) +
        ")");
  }

  LOG_INFO("RealmLink: connected to auth {}:{} (realm {})", host, port, realmId);

  boost::asio::steady_timer tick(socket.get_executor());

  while (!stop) {
    for (int i = 0; i < 40 && !stop; ++i) {
      tick.expires_after(std::chrono::milliseconds(250));
      boost::system::error_code ec;
      co_await tick.async_wait(boost::asio::redirect_error(Asio::use_awaitable, ec));
      if (ec == boost::asio::error::operation_aborted)
        co_return;
    }
    if (stop)
      co_return;

    uint8_t ping = kRealmLinkPing;
    co_await Asio::AsyncWrite(socket, std::span<const uint8>(&ping, 1));
  }
}

} // namespace

void RunRealmLinkOutbound(const Config &config, std::atomic<bool> &stop) {
  std::string const host =
      config.GetNestedScalarString({"RealmLink", "AuthHost"}, "127.0.0.1");
  uint16_t const port = static_cast<uint16_t>(
      config.GetNested<int>({"RealmLink", "AuthPort"}, 3725));
  std::string token =
      config.GetNestedScalarString({"RealmLink", "Token"}, "");
  if (token.empty())
    token = config.GetNestedScalarString({"RealmLink", "token"}, "");
  uint32_t const realmId = static_cast<uint32_t>(
      config.GetNested<int>({"RealmLink", "RealmId"}, 1));

  if (token.empty()) {
    LOG_ERROR("RealmLink: Token is empty; outbound link disabled.");
    return;
  }

  while (!stop) {
    try {
      boost::asio::io_context io;
      tcp::socket socket(io);
      tcp::resolver resolver(io);
      auto endpoints = resolver.resolve(host, std::to_string(port));
      boost::asio::connect(socket, endpoints);
      applyTcpIoTimeouts(socket);

      std::vector<uint8> handshake;
      appendU32Le(handshake, kRealmLinkMagic);
      uint16_t const tokenLen = static_cast<uint16_t>(
          std::min(token.size(), static_cast<size_t>(kRealmLinkMaxTokenLen)));
      appendU16Le(handshake, tokenLen);
      handshake.insert(handshake.end(), token.begin(),
                       token.begin() + tokenLen);
      appendU32Le(handshake, realmId);

      boost::asio::co_spawn(
          io,
          [&]() -> Asio::awaitable<void> {
            co_await OutboundLinkSession(socket, stop, handshake, host, port,
                                       realmId);
          },
          Asio::detached);

      io.run();
    } catch (const std::exception &e) {
      if (stop)
        break;
      LOG_WARN("RealmLink: {} — reconnecting in 2s", e.what());
      std::this_thread::sleep_for(std::chrono::seconds(2));
    }
  }
}

} // namespace Firelands
