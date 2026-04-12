#ifndef FIRELANDS_APPLICATION_SERVICES_AUTH_SERVICE_H
#define FIRELANDS_APPLICATION_SERVICES_AUTH_SERVICE_H

#include <domain/repositories/IAccountRepository.h>
#include <shared/Crypto.h>
#include <memory>

namespace Firelands {

    class AuthService {
    public:
        explicit AuthService(std::shared_ptr<IAccountRepository> accountRepo)
            : _accountRepo(std::move(accountRepo)) {}


        std::optional<Account> FindAccount(const std::string& username) {
            return _accountRepo->FindByUsername(Crypto::ToUpper(username));
        }

    private:
        std::shared_ptr<IAccountRepository> _accountRepo;
    };

} // namespace Firelands

#endif // FIRELANDS_APPLICATION_SERVICES_AUTH_SERVICE_H
