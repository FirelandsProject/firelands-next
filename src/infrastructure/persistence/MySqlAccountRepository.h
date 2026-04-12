#ifndef FIRELANDS_INFRASTRUCTURE_PERSISTENCE_MYSQL_ACCOUNT_REPOSITORY_H
#define FIRELANDS_INFRASTRUCTURE_PERSISTENCE_MYSQL_ACCOUNT_REPOSITORY_H

#include <domain/repositories/IAccountRepository.h>
#include <memory>

// Forward declaration for MariaDB/JDBC types
namespace sql { 
    class Connection; 
}

namespace Firelands {

    class MySqlAccountRepository : public IAccountRepository {
    public:
        explicit MySqlAccountRepository(std::shared_ptr<sql::Connection> connection);

        std::optional<Account> FindByUsername(const std::string& username) override;
        void Create(const Account& account) override;
        void Update(const Account& account) override;

    private:
        std::shared_ptr<sql::Connection> _connection;
    };

} // namespace Firelands

#endif // FIRELANDS_INFRASTRUCTURE_PERSISTENCE_MYSQL_ACCOUNT_REPOSITORY_H
