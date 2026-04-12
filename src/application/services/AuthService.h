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

        bool Authenticate(const std::string& username, const std::string& password) {
            auto account = _accountRepo->FindByUsername(Crypto::ToUpper(username));
            
            if (!account) {
                return false;
            }

            // WoW SRP/Auth Rule: SHA1(UPPER(username) + ":" + UPPER(password))
            std::string calculatedHash = Crypto::ToHexString(
                Crypto::CalculateSHA1(Crypto::ToUpper(username) + ":" + Crypto::ToUpper(password))
            );
            
            return (calculatedHash == account->shaPassHash);
        }

    private:
        std::shared_ptr<IAccountRepository> _accountRepo;
    };

} // namespace Firelands

#endif // FIRELANDS_APPLICATION_SERVICES_AUTH_SERVICE_H
