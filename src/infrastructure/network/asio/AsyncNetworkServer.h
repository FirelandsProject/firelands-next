#ifndef FIRELANDS_INFRASTRUCTURE_NETWORK_ASIO_ASYNC_NETWORK_SERVER_H
#define FIRELANDS_INFRASTRUCTURE_NETWORK_ASIO_ASYNC_NETWORK_SERVER_H

#include <application/ports/INetworkServer.h>
#include <application/services/AuthService.h>
#include <boost/asio.hpp>
#include <memory>
#include <string>

namespace Firelands {

    using boost::asio::ip::tcp;

    class AsyncNetworkServer : public INetworkServer {
    public:
        AsyncNetworkServer(std::shared_ptr<AuthService> authService);
        ~AsyncNetworkServer() override;

        bool Start(const std::string& address, uint16 port) override;
        void Stop() override;
        void Update() override;

    private:
        void DoAccept();

        boost::asio::io_context _ioContext;
        std::unique_ptr<tcp::acceptor> _acceptor;
        std::shared_ptr<AuthService> _authService;
    };

} // namespace Firelands

#endif // FIRELANDS_INFRASTRUCTURE_NETWORK_ASIO_ASYNC_NETWORK_SERVER_H
