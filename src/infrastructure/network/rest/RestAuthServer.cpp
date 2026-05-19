#include <boost/beast/version.hpp>
#include <infrastructure/network/asio/AsioAwaitables.h>
#include <infrastructure/network/rest/RestAuthServer.h>
#include <nlohmann/json.hpp>
#include <shared/Logger.h>

namespace Firelands {

using json = nlohmann::json;

RestAuthServer::RestAuthServer(
    std::shared_ptr<AuthService> authService,
    std::shared_ptr<WebSessionService> webSessionService,
    const std::string &address, uint16_t port)
    : _authService(std::move(authService)),
      _webSessionService(std::move(webSessionService)), _address(address),
      _port(port) {}

RestAuthServer::~RestAuthServer() { Stop(); }

bool RestAuthServer::Start() {
  try {
    auto const address = net::ip::make_address(_address);

    _acceptor =
        std::make_unique<tcp::acceptor>(_ioc, tcp::endpoint{address, _port});
    _running = true;

    Asio::SpawnDetached(_ioc.get_executor(), [this]() -> Asio::awaitable<void> {
      co_await AcceptLoop();
    });

    _workerThread = std::thread([this]() {
      LOG_DEBUG("REST Auth Server running on {}:{}", _address, _port);
      _ioc.run();
    });

    return true;
  } catch (const std::exception &e) {
    LOG_ERROR("Failed to start REST Auth Server: {}", e.what());
    return false;
  }
}

void RestAuthServer::Stop() {
  if (!_running)
    return;

  _running = false;
  boost::system::error_code ec;
  if (_acceptor) {
    _acceptor->close(ec);
  }
  _ioc.stop();
  if (_workerThread.joinable()) {
    _workerThread.join();
  }
}

Asio::awaitable<void> RestAuthServer::AcceptLoop() {
  while (_running) {
    try {
      tcp::socket socket = co_await _acceptor->async_accept(Asio::use_awaitable);
      if (!_running)
        break;

      Asio::SpawnDetached(
          socket.get_executor(),
          [this, socket = std::move(socket)]() mutable -> Asio::awaitable<void> {
            co_await ServeClient(std::move(socket));
          });
    } catch (const boost::system::system_error &e) {
      if (e.code() == boost::asio::error::operation_aborted || !_running) {
        co_return;
      }
      LOG_WARN("REST accept error: {}", e.what());
    }
  }
}

Asio::awaitable<void> RestAuthServer::ServeClient(tcp::socket socket) {
  try {
    beast::flat_buffer buffer;
    http::request<http::string_body> req;
    co_await http::async_read(socket, buffer, req, Asio::use_awaitable);

    http::response<http::string_body> res = BuildResponse(req);
    res.prepare_payload();
    co_await http::async_write(socket, res, Asio::use_awaitable);

    beast::error_code ec;
    socket.shutdown(tcp::socket::shutdown_send, ec);
  } catch (const std::exception &e) {
    LOG_ERROR("REST Request Error: {}", e.what());
  }
}

http::response<http::string_body>
RestAuthServer::BuildResponse(http::request<http::string_body> const &req) const {
  http::response<http::string_body> res;
  res.version(req.version());
  res.keep_alive(false);
  res.set(http::field::server, "Firelands Auth REST");
  res.set(http::field::content_type, "application/json");

  if (req.method() == http::verb::post && req.target() == "/auth/login") {
    try {
      auto data = json::parse(req.body());
      std::string username = data.value("username", "");
      std::string password = data.value("password", "");

      if (username.empty() || password.empty()) {
        res.result(http::status::bad_request);
        res.body() = json({{"error", "Missing username or password"}}).dump();
      } else {
        bool valid = _authService->VerifyCredentials(username, password);

        if (valid) {
          auto account = _authService->FindAccount(username);
          auto session = _webSessionService->CreateSession(account->id);

          res.result(http::status::ok);
          res.body() =
              json({{"success", true}, {"token", session.token}}).dump();

          LOG_DEBUG("REST successful login for user: {}", username);
        } else {
          res.result(http::status::unauthorized);
          res.body() =
              json({{"success", false}, {"error", "Invalid credentials"}})
                  .dump();
          LOG_WARN("REST failed login attempt for user: {}", username);
        }
      }
    } catch (const std::exception & /*e*/) {
      res.result(http::status::bad_request);
      res.body() = json({{"error", "Invalid JSON"}}).dump();
    }
  } else if (req.method() == http::verb::get &&
             req.target() == "/auth/verify") {
    auto it = req.find(http::field::authorization);
    if (it == req.end()) {
      res.result(http::status::unauthorized);
      res.body() = json({{"error", "Missing Authorization header"}}).dump();
    } else {
      std::string authVal(it->value());
      std::string token;
      if (authVal.substr(0, 7) == "Bearer ") {
        token = authVal.substr(7);
      } else {
        token = authVal;
      }

      auto session = _webSessionService->ValidateToken(token);
      if (session) {
        res.result(http::status::ok);
        res.body() =
            json({{"valid", true}, {"accountId", session->accountId}}).dump();
      } else {
        res.result(http::status::unauthorized);
        res.body() =
            json({{"valid", false}, {"error", "Invalid or expired token"}})
                .dump();
      }
    }
  } else {
    res.result(http::status::not_found);
    res.body() = json({{"error", "Not Found"}}).dump();
  }

  return res;
}

} // namespace Firelands
