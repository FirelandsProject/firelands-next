#include <gtest/gtest.h>
#include <infrastructure/persistence/MySqlAccountRepository.h>
#include <infrastructure/persistence/DatabaseService.h>
#include <memory>

namespace Firelands {
namespace {

    class MySqlAccountRepositoryTest : public ::testing::Test {
    protected:
        void SetUp() override {
            // These parameters should match docker-compose.yml
            // In a real environment, use environment variables or a config file
            std::string url = "tcp://127.0.0.1:3306/firelands_auth";
            std::string user = "root";
            std::string pass = "root";

            try {
                DatabaseService dbService(url, user, pass);
                _connection = dbService.CreateConnection();
                _repository = std::make_unique<MySqlAccountRepository>(_connection);
                
                // Cleanup before each test
                auto stmt = _connection->createStatement();
                stmt->execute("DELETE FROM account WHERE username = 'test_user'");
            } catch (const std::exception& e) {
                GTEST_SKIP() << "Database connection failed. Is Docker running? Error: " << e.what();
            }
        }

        std::shared_ptr<sql::Connection> _connection;
        std::unique_ptr<MySqlAccountRepository> _repository;
    };

    TEST_F(MySqlAccountRepositoryTest, CanCreateAndFindAccount) {
        Account newAcc;
        newAcc.username = "test_user";
        newAcc.email = "test@firelands.com";
        newAcc.expansion = 3;

        _repository->Create(newAcc);

        auto found = _repository->FindByUsername("test_user");

        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(found->username, "test_user");
        EXPECT_EQ(found->email, "test@firelands.com");
        EXPECT_EQ(found->expansion, 3);
    }

} // namespace
} // namespace Firelands
