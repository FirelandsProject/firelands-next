#include <infrastructure/network/realm_link/RealmLinkOutbound.h>
#include <shared/Config.h>
#include <shared/Logger.h>
#include <shared/network/RealmLinkProtocol.h>

#include <algorithm>
#include <boost/asio.hpp>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

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

} // namespace

void RunRealmLinkOutbound(const Config &config, std::atomic<bool> &stop) {
  using boost::asio::ip::tcp;

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

      std::vector<uint8> handshake;
      appendU32Le(handshake, kRealmLinkMagic);
      uint16_t const tokenLen = static_cast<uint16_t>(
          std::min(token.size(), static_cast<size_t>(kRealmLinkMaxTokenLen)));
      appendU16Le(handshake, tokenLen);
      handshake.insert(handshake.end(), token.begin(),
                         token.begin() + tokenLen);
      appendU32Le(handshake, realmId);

      boost::asio::write(socket, boost::asio::buffer(handshake));

      uint8_t ack = kRealmLinkAckReject;
      boost::asio::read(socket, boost::asio::buffer(&ack, 1));
      if (ack != kRealmLinkAckOk) {
        LOG_WARN("RealmLink: auth rejected handshake (ack={}); retrying…",
                 static_cast<int>(ack));
        std::this_thread::sleep_for(std::chrono::seconds(2));
        continue;
      }

      LOG_INFO("RealmLink: connected to auth {}:{} (realm {})", host, port,
               realmId);

      while (!stop) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        uint8_t ping = kRealmLinkPing;
        boost::asio::write(socket, boost::asio::buffer(&ping, 1));
      }
    } catch (const std::exception &e) {
      if (stop)
        break;
      LOG_WARN("RealmLink: {} — reconnecting in 2s", e.what());
      std::this_thread::sleep_for(std::chrono::seconds(2));
    }
  }
}

} // namespace Firelands
