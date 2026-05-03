#include <infrastructure/network/asio/AsyncNetworkServer.h>
#include <infrastructure/network/sessions/AuthSession.h>
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

    DoAccept();
    return true;
  } catch (std::exception &e) {
    LOG_ERROR("Failed to start network server: {}", e.what());
    return false;
  }
}

void AsyncNetworkServer::Stop() {
  _ioContext.stop();
  if (_acceptor) {
    _acceptor->close();
  }
}

void AsyncNetworkServer::Update() { _ioContext.poll(); }

void AsyncNetworkServer::DoAccept() {
  _acceptor->async_accept(
      [this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec) {
          LOG_DEBUG("New TCP connection from {}",
                    socket.remote_endpoint().address().to_string());
          if (_sessionFactory) {
            _sessionFactory(std::move(socket));
          }
        }

        DoAccept();
      });
}

} // namespace Firelands
