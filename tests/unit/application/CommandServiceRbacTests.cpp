#include <application/ports/ICommandSession.h>
#include <application/services/CommandService.h>
#include <domain/repositories/IAccountRepository.h>
#include <domain/repositories/IRbacRepository.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <shared/Logger.h>
#include <shared/game/Permissions.h>
#include <shared/network/MovementInfo.h>

namespace Firelands {
namespace {

class MockRbacRepository : public IRbacRepository {
public:
  MOCK_METHOD(std::vector<RbacRole>, ListRoles, (), (override));
  MOCK_METHOD(std::optional<RbacRole>, FindRoleByName, (std::string const &),
              (override));
  MOCK_METHOD(std::optional<RbacRole>, FindRoleById, (uint32_t), (override));
  MOCK_METHOD(std::optional<uint32_t>, CreateRole,
              (std::string const &, uint64_t), (override));
  MOCK_METHOD(bool, UpdateRolePermissionMask, (uint32_t, uint64_t), (override));
  MOCK_METHOD(bool, DeleteRoleByName, (std::string const &), (override));
  MOCK_METHOD(std::vector<RbacRole>, ListRolesForAccount, (uint32_t), (override));
  MOCK_METHOD(bool, GrantRoleToAccount, (uint32_t, uint32_t), (override));
  MOCK_METHOD(bool, RevokeRoleFromAccount, (uint32_t, uint32_t), (override));
  MOCK_METHOD(uint64_t, UnionPermissionMaskForAccount, (uint32_t), (override));
  MOCK_METHOD(void, EnsureBuiltinRoles, (), (override));
  MOCK_METHOD(bool, SetPrimaryStaffRole, (uint32_t, std::string const &),
              (override));
};

class MockAccountRepository : public IAccountRepository {
public:
  MOCK_METHOD(std::optional<Account>, FindByUsername, (std::string const &),
              (override));
  MOCK_METHOD(void, Create, (Account const &), (override));
  MOCK_METHOD(void, Update, (Account const &), (override));
  MOCK_METHOD(void, DeleteByUsername, (std::string const &), (override));
  MOCK_METHOD(void, CreateSession, (uint32, std::vector<uint8> const &),
              (override));
  MOCK_METHOD(std::vector<uint8>, GetSessionKey, (uint32), (override));
  MOCK_METHOD(void, SetLockedByUsername, (std::string const &, bool), (override));
};

class ConsoleSession : public ICommandSession {
public:
  MovementInfo const &GetPosition() const override { return m_pos; }
  void TeleportTo(uint32_t, float, float, float, float) override {}
  void SendNotification(std::string const &message) override {
    notifications.push_back(message);
  }

  MovementInfo m_pos{};
  std::vector<std::string> notifications;
};

class CommandServiceRbacTests : public ::testing::Test {
protected:
  static void SetUpTestSuite() {
    Logger::Init(LoggerBuilder().WithConsole(false).Build());
  }
};

TEST_F(CommandServiceRbacTests, RoleListRequiresRepository) {
  CommandService service;
  auto session = std::make_shared<ConsoleSession>();
  EXPECT_TRUE(service.ExecuteCommand(session, ".rbac role list",
                                    PrivilegeOrigin::ServerConsole));
  ASSERT_FALSE(session->notifications.empty());
  EXPECT_NE(session->notifications.back().find("not configured"),
            std::string::npos);
}

TEST_F(CommandServiceRbacTests, RoleCreateStoresNormalizedPermissions) {
  auto rbac = std::make_shared<MockRbacRepository>();
  CommandService service({}, {}, {}, {}, rbac);
  auto session = std::make_shared<ConsoleSession>();

  EXPECT_CALL(*rbac, CreateRole("moderator_tools", ToMask(Permission::CommandGps)))
      .WillOnce(testing::Return(7u));

  EXPECT_TRUE(service.ExecuteCommand(
      session, ".rbac role create Moderator_Tools gps",
      PrivilegeOrigin::ServerConsole));
  ASSERT_FALSE(session->notifications.empty());
  EXPECT_NE(session->notifications.back().find("Created role"), std::string::npos);
}

TEST_F(CommandServiceRbacTests, GrantLinksAccountToRole) {
  auto rbac = std::make_shared<MockRbacRepository>();
  auto accounts = std::make_shared<MockAccountRepository>();
  CommandService service({}, accounts, {}, {}, rbac);
  auto session = std::make_shared<ConsoleSession>();

  Account acc;
  acc.id = 42;
  acc.username = "STAFF";
  acc.accessLevel = AccessLevel::Player;

  RbacRole role;
  role.id = 3;
  role.name = "support";

  EXPECT_CALL(*accounts, FindByUsername("STAFF")).WillOnce(testing::Return(acc));
  EXPECT_CALL(*rbac, FindRoleByName("support")).WillOnce(testing::Return(role));
  EXPECT_CALL(*rbac, GrantRoleToAccount(42, 3)).WillOnce(testing::Return(true));

  EXPECT_TRUE(service.ExecuteCommand(session, ".rbac grant STAFF support",
                                    PrivilegeOrigin::ServerConsole));
  ASSERT_FALSE(session->notifications.empty());
  EXPECT_NE(session->notifications.back().find("Granted"), std::string::npos);
}

TEST_F(CommandServiceRbacTests, SetStaffAssignsBuiltinRole) {
  auto rbac = std::make_shared<MockRbacRepository>();
  auto accounts = std::make_shared<MockAccountRepository>();
  CommandService service({}, accounts, {}, {}, rbac);
  auto session = std::make_shared<ConsoleSession>();

  Account acc;
  acc.id = 9;
  acc.username = "BOB";

  EXPECT_CALL(*accounts, FindByUsername("BOB")).WillOnce(testing::Return(acc));
  EXPECT_CALL(*rbac, SetPrimaryStaffRole(9, "gamemaster"))
      .WillOnce(testing::Return(true));

  EXPECT_TRUE(service.ExecuteCommand(session, ".rbac setstaff BOB gamemaster",
                                    PrivilegeOrigin::ServerConsole));
}

} // namespace
} // namespace Firelands
