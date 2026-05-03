#include <gtest/gtest.h>
#include <shared/game/Permissions.h>

using namespace Firelands;

TEST(PermissionsTests, PlayerLacksGps) {
  EXPECT_FALSE(HasPermission(AccessLevel::Player, PrivilegeOrigin::GameClient,
                             ToMask(Permission::CommandGps)));
}

TEST(PermissionsTests, ModeratorHasGps) {
  EXPECT_TRUE(HasPermission(AccessLevel::Moderator, PrivilegeOrigin::GameClient,
                            ToMask(Permission::CommandGps)));
}

TEST(PermissionsTests, ModeratorHasMailboxCommand) {
  EXPECT_TRUE(HasPermission(AccessLevel::Moderator, PrivilegeOrigin::GameClient,
                            ToMask(Permission::CommandMailbox)));
}

TEST(PermissionsTests, GameMasterHasTeleport) {
  EXPECT_TRUE(HasPermission(AccessLevel::GameMaster, PrivilegeOrigin::GameClient,
                             ToMask(Permission::CommandTeleport)));
}

TEST(PermissionsTests, ConsoleGrantsTeleportToPlayerAccount) {
  EXPECT_TRUE(HasPermission(AccessLevel::Player, PrivilegeOrigin::ServerConsole,
                             ToMask(Permission::CommandTeleport)));
}

TEST(PermissionsTests, ZeroRequiredAlwaysTrue) {
  EXPECT_TRUE(HasPermission(AccessLevel::Player, PrivilegeOrigin::GameClient, 0));
}

TEST(PermissionsTests, GameMasterHasGameplayCommands) {
  EXPECT_TRUE(HasPermission(AccessLevel::GameMaster, PrivilegeOrigin::GameClient,
                            ToMask(Permission::CommandGameplay)));
}

TEST(PermissionsTests, GameMasterHasGmTickets) {
  EXPECT_TRUE(HasPermission(AccessLevel::GameMaster, PrivilegeOrigin::GameClient,
                            ToMask(Permission::ManageGmTickets)));
}
