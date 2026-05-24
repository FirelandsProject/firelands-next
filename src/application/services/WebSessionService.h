#ifndef FIRELANDS_APPLICATION_SERVICES_WEB_SESSION_SERVICE_H
#define FIRELANDS_APPLICATION_SERVICES_WEB_SESSION_SERVICE_H

#include <domain/repositories/IWebSessionRepository.h>
#include <memory>
#include <random>
#include <shared/Crypto.h>

namespace Firelands {

class WebSessionService {
public:
  explicit WebSessionService(std::shared_ptr<IWebSessionRepository> sessionRepo)
      : _sessionRepo(std::move(sessionRepo)) {}

  WebSession
  CreateSession(uint32 accountId,
                std::chrono::hours duration = std::chrono::hours(24)) {
    WebSession session;
    session.accountId = accountId;
    session.token = GenerateToken();
    session.expiresAt = std::chrono::system_clock::now() + duration;

    _sessionRepo->Save(session);
    return session;
  }

  std::optional<WebSession> ValidateToken(const std::string &token) {
    auto session = _sessionRepo->FindByToken(token);
    if (session && !session->IsExpired()) {
      return session;
    }
    if (session) {
      _sessionRepo->DeleteByToken(token);
    }
    return std::nullopt;
  }

  void Logout(const std::string &token) { _sessionRepo->DeleteByToken(token); }

private:
  std::string GenerateToken() {
    // Generate a random 32-byte token and return hex representation
    std::vector<uint8_t> randomBytes(32);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 255);
    std::generate(randomBytes.begin(), randomBytes.end(),
                  [&]() { return static_cast<uint8_t>(dis(gen)); });

    return Crypto::ToHexString(Crypto::CalculateSHA1(randomBytes));
  }

  std::shared_ptr<IWebSessionRepository> _sessionRepo;
};

} // namespace Firelands

#endif // FIRELANDS_APPLICATION_SERVICES_WEB_SESSION_SERVICE_H
