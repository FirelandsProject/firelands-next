#ifndef FIRELANDS_INFRASTRUCTURE_NETWORK_ASIO_ASYNC_NETWORK_SERVER_H
#define FIRELANDS_INFRASTRUCTURE_NETWORK_ASIO_ASYNC_NETWORK_SERVER_H

#include <application/ports/INetworkServer.h>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace Firelands {

using boost::asio::ip::tcp;

class AsyncNetworkServer : public INetworkServer {
public:
  using SessionFactory = std::function<void(tcp::socket)>;

  explicit AsyncNetworkServer(SessionFactory sessionFactory);
  ~AsyncNetworkServer() override;

  bool Start(const std::string &address, uint16 port) override;
  void Stop() override;
  void Update() override;

private:
  boost::asio::awaitable<void> AcceptLoop();

  boost::asio::io_context _ioContext;
  std::unique_ptr<tcp::acceptor> _acceptor;
  SessionFactory _sessionFactory;
  std::atomic<bool> _running{false};
};

} // namespace Firelands

#endif // FIRELANDS_INFRASTRUCTURE_NETWORK_ASIO_ASYNC_NETWORK_SERVER_H
