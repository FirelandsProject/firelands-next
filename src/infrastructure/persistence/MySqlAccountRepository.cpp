#include "MySqlAccountRepository.h"
#include <conncpp.hpp>
#include <sstream>
#include <shared/Logger.h>

namespace Firelands {

    MySqlAccountRepository::MySqlAccountRepository(std::shared_ptr<sql::Connection> connection)
        : _connection(std::move(connection)) {}

    std::optional<Account> MySqlAccountRepository::FindByUsername(const std::string& username) {
        try {
            std::shared_ptr<sql::PreparedStatement> stmnt(
                _connection->prepareStatement("SELECT id, username, email, salt, verifier, expansion FROM account WHERE username = ?")
            );
            stmnt->setString(1, username);
            
            std::unique_ptr<sql::ResultSet> res(stmnt->executeQuery());
            
            if (res->next()) {
                Account acc;
                acc.id = res->getInt("id");
                acc.username = res->getString("username");
                acc.email = res->getString("email");
                
                // Read Binary Salt
                auto saltStream = res->getBlob("salt");
                acc.salt.assign(std::istreambuf_iterator<char>(*saltStream), std::istreambuf_iterator<char>());
                
                // Read Binary Verifier
                auto verifierStream = res->getBlob("verifier");
                acc.verifier.assign(std::istreambuf_iterator<char>(*verifierStream), std::istreambuf_iterator<char>());

                acc.expansion = static_cast<uint8>(res->getInt("expansion"));
                return acc;
            }
        } catch (sql::SQLException& e) {
            LOG_ERROR("Database error in FindByUsername: {}", e.what());
            return std::nullopt;
        }
        
        return std::nullopt;
    }

    void MySqlAccountRepository::Create(const Account& account) {
        try {
            std::shared_ptr<sql::PreparedStatement> stmnt(
                _connection->prepareStatement("INSERT INTO account (username, email, salt, verifier, expansion) VALUES (?, ?, ?, ?, ?)")
            );
            stmnt->setString(1, account.username);
            stmnt->setString(2, account.email);
            
            // Set Salt as Blob
            std::string saltStr(account.salt.begin(), account.salt.end());
            std::istringstream saltStream(saltStr);
            stmnt->setBlob(3, &saltStream);

            // Set Verifier as Blob
            std::string verifierStr(account.verifier.begin(), account.verifier.end());
            std::istringstream verifierStream(verifierStr);
            stmnt->setBlob(4, &verifierStream);

            stmnt->setInt(5, account.expansion);
            stmnt->executeUpdate();
        } catch (sql::SQLException& e) {
            LOG_ERROR("Database error in Create: {}", e.what());
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
            LOG_ERROR("Database error in Update: {}", e.what());
        }
    }

    void MySqlAccountRepository::DeleteByUsername(const std::string& username) {
        try {
            std::shared_ptr<sql::PreparedStatement> stmnt(
                _connection->prepareStatement("DELETE FROM account WHERE username = ?")
            );
            stmnt->setString(1, username);
            stmnt->executeUpdate();
        } catch (sql::SQLException& e) {
            LOG_ERROR("Database error in DeleteByUsername: {}", e.what());
        }
    }

} // namespace Firelands
