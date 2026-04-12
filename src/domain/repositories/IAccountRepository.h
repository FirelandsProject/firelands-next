#ifndef FIRELANDS_DOMAIN_REPOSITORIES_IACCOUNT_REPOSITORY_H
#define FIRELANDS_DOMAIN_REPOSITORIES_IACCOUNT_REPOSITORY_H

#include <shared/Common.h>
#include <string>
#include <optional>
#include <vector>

namespace Firelands {

    struct Account {
        uint32 id;
        std::string username;
        std::string email;
        std::string shaPassHash;
        uint8 expansion;
    };

    class IAccountRepository {
    public:
        virtual ~IAccountRepository() = default;

        virtual std::optional<Account> FindByUsername(const std::string& username) = 0;
        virtual void Create(const Account& account) = 0;
        virtual void Update(const Account& account) = 0;
    };

} // namespace Firelands

#endif // FIRELANDS_DOMAIN_REPOSITORIES_IACCOUNT_REPOSITORY_H
