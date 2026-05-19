#include <infrastructure/network/asio/AsioAwaitables.h>
#include <infrastructure/network/asio/AsyncNetworkServer.h>
#include <shared/Logger.h>

namespace Firelands {

AsyncNetworkServer::AsyncNetworkServer(SessionFactory sessionFactory)
    : _sessionFactory(std::move(sessionFactory)) {}

AsyncNetworkServer::~AsyncNetworkServer() { Stop(); }

bool AsyncNetworkServer::Start(const std::string &address, uint16 port) {
  try {
    tcp::endpoint endpoint(boost::asio::ip::make_address(address), port);
    _acceptor = std::make_unique<tcp::acceptor>(_ioContext, endpoint);

    LOG_DEBUG("Network server acceptor bound on {}:{}", address, port);

    _running = true;
    Asio::SpawnDetached(_ioContext.get_executor(),
                        [this]() -> Asio::awaitable<void> {
                          co_await AcceptLoop();
                        });
    return true;
  } catch (std::exception &e) {
    LOG_ERROR("Failed to start network server: {}", e.what());
    return false;
  }
}

void AsyncNetworkServer::Stop() {
  _running = false;
  if (_acceptor) {
    boost::system::error_code ec;
    _acceptor->close(ec);
  }
  _ioContext.stop();
}

void AsyncNetworkServer::Update() { _ioContext.poll(); }

Asio::awaitable<void> AsyncNetworkServer::AcceptLoop() {
  while (_running) {
    try {
      tcp::socket socket = co_await _acceptor->async_accept(Asio::use_awaitable);
      if (!_running)
        break;

      LOG_DEBUG("New TCP connection from {}",
                socket.remote_endpoint().address().to_string());
      if (_sessionFactory) {
        _sessionFactory(std::move(socket));
      }
    } catch (const boost::system::system_error &e) {
      if (e.code() == boost::asio::error::operation_aborted ||
          !_running) {
        co_return;
      }
      LOG_WARN("Accept error: {}", e.what());
    }
  }
}

} // namespace Firelands
