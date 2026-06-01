#include <application/ports/ICommandSession.h>
#include <application/services/CommandService.h>
#include <gtest/gtest.h>
#include <shared/Logger.h>
#include <shared/game/Permissions.h>
#include <shared/network/MovementInfo.h>

namespace Firelands {
namespace {

class CommandServiceReviveUnlearnTests : public ::testing::Test {
protected:
  static void SetUpTestSuite() {
    Logger::Init(LoggerBuilder().WithConsole(false).Build());
  }
};

class MockCommandSession : public ICommandSession {
public:
  MovementInfo const &GetPosition() const override { return m_pos; }
  PermissionMask GetAccountRolePermissionMask() const override { return m_roleMask; }
  void TeleportTo(uint32_t, float, float, float, float) override {}

  void SendNotification(const std::string &message) override {
    notifications.push_back(message);
  }

  bool GmUnlearnSpell(uint32 spellId) override {
    lastUnlearnSpellId = spellId;
    return unlearnResult;
  }

  bool GmRevivePlayer(uint64 playerGuid) override {
    lastReviveTargetGuid = playerGuid;
    return revivePlayerResult;
  }

  bool GmReviveSelf() override {
    reviveSelfCalled = true;
    return reviveSelfResult;
  }

  uint64_t GetClientSelectionGuid() const override { return selectionGuid; }

  MovementInfo m_pos{};
  PermissionMask m_roleMask = DefaultPermissions(AccessLevel::GameMaster);
  std::vector<std::string> notifications;
  uint64_t selectionGuid = 0;
  uint32 lastUnlearnSpellId = 0;
  bool unlearnResult = false;
  uint64_t lastReviveTargetGuid = 0;
  bool revivePlayerResult = false;
  bool reviveSelfCalled = false;
  bool reviveSelfResult = true;
};

TEST_F(CommandServiceReviveUnlearnTests, UnlearnFailureWhenSpellNotKnown) {
  CommandService service;
  auto session = std::make_shared<MockCommandSession>();
  session->unlearnResult = false;

  EXPECT_FALSE(service.ExecuteCommand(session, ".unlearn 12345", PrivilegeOrigin::GameClient));
  EXPECT_EQ(session->lastUnlearnSpellId, 12345u);
  ASSERT_FALSE(session->notifications.empty());
  EXPECT_NE(session->notifications.back().find("Unlearn failed"), std::string::npos);
}

TEST_F(CommandServiceReviveUnlearnTests, UnlearnSuccessWhenSpellRemoved) {
  CommandService service;
  auto session = std::make_shared<MockCommandSession>();
  session->unlearnResult = true;

  EXPECT_TRUE(service.ExecuteCommand(session, ".unlearn 100", PrivilegeOrigin::GameClient));
}

TEST_F(CommandServiceReviveUnlearnTests, ReviveSelectedPlayerDoesNotFallBackToSelf) {
  CommandService service;
  auto session = std::make_shared<MockCommandSession>();
  session->selectionGuid = 0x2000;
  session->revivePlayerResult = false;

  EXPECT_FALSE(service.ExecuteCommand(session, ".revive", PrivilegeOrigin::GameClient));
  EXPECT_FALSE(session->reviveSelfCalled);
  ASSERT_FALSE(session->notifications.empty());
  EXPECT_NE(session->notifications.back().find("select a player"), std::string::npos);
}

TEST_F(CommandServiceReviveUnlearnTests, ReviveSelfWhenNoSelection) {
  CommandService service;
  auto session = std::make_shared<MockCommandSession>();
  session->selectionGuid = 0;
  session->reviveSelfResult = true;

  EXPECT_TRUE(service.ExecuteCommand(session, ".revive", PrivilegeOrigin::GameClient));
  EXPECT_TRUE(session->reviveSelfCalled);
}

TEST_F(CommandServiceReviveUnlearnTests, ReviveSelectedPlayerWhenReviveSucceeds) {
  CommandService service;
  auto session = std::make_shared<MockCommandSession>();
  session->selectionGuid = 0x3000;
  session->revivePlayerResult = true;

  EXPECT_TRUE(service.ExecuteCommand(session, ".revive", PrivilegeOrigin::GameClient));
  EXPECT_EQ(session->lastReviveTargetGuid, 0x3000u);
  EXPECT_FALSE(session->reviveSelfCalled);
}

} // namespace
} // namespace Firelands
