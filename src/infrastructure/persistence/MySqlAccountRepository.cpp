#include "MySqlAccountRepository.h"
#include <conncpp.hpp>

namespace Firelands {

    MySqlAccountRepository::MySqlAccountRepository(std::shared_ptr<sql::Connection> connection)
        : _connection(std::move(connection)) {}

    std::optional<Account> MySqlAccountRepository::FindByUsername(const std::string& username) {
        try {
            std::shared_ptr<sql::PreparedStatement> stmnt(
                _connection->prepareStatement("SELECT id, username, email, sha_pass_hash, expansion FROM account WHERE username = ?")
            );
            stmnt->setString(1, username);
            
            std::unique_ptr<sql::ResultSet> res(stmnt->executeQuery());
            
            if (res->next()) {
                Account acc;
                acc.id = res->getInt("id");
                acc.username = res->getString("username");
                acc.email = res->getString("email");
                acc.shaPassHash = res->getString("sha_pass_hash");
                acc.expansion = static_cast<uint8>(res->getInt("expansion"));
                return acc;
            }
        } catch (sql::SQLException& e) {
            // Skill-01: Log in English
            // TODO: Use a proper Logger in infrastructure
            return std::nullopt;
        }
        
        return std::nullopt;
    }

    void MySqlAccountRepository::Create(const Account& account) {
        try {
            std::shared_ptr<sql::PreparedStatement> stmnt(
                _connection->prepareStatement("INSERT INTO account (username, email, expansion) VALUES (?, ?, ?)")
            );
            stmnt->setString(1, account.username);
            stmnt->setString(2, account.email);
            stmnt->setInt(3, account.expansion);
            stmnt->executeUpdate();
        } catch (sql::SQLException& e) {
            // TODO: Error handling
        }
    }

    void MySqlAccountRepository::Update(const Account& account) {
        try {
            std::shared_ptr<sql::PreparedStatement> stmnt(
                _connection->prepareStatement("UPDATE account SET email = ?, expansion = ? WHERE id = ?")
            );
            stmnt->setString(1, account.email);
            stmnt->setInt(2, account.expansion);
            stmnt->setInt(3, account.id);
            stmnt->executeUpdate();
        } catch (sql::SQLException& e) {
            // TODO: Error handling
        }
    }

} // namespace Firelands
