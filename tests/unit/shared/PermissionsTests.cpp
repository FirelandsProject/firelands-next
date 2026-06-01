#include <gtest/gtest.h>
#include <shared/game/Permissions.h>
#include <shared/game/RbacBuiltinRoles.h>

using namespace Firelands;

TEST(PermissionsTests, PlayerLacksGpsWithoutRoles) {
  EXPECT_FALSE(HasPermission(PrivilegeOrigin::GameClient, ToMask(Permission::CommandGps),
                             0));
}

TEST(PermissionsTests, ModeratorRoleMaskHasGps) {
  PermissionMask const mod = DefaultPermissions(AccessLevel::Moderator);
  EXPECT_TRUE(HasPermission(PrivilegeOrigin::GameClient, ToMask(Permission::CommandGps),
                            mod));
}

TEST(PermissionsTests, ConsoleGrantsAllPermissions) {
  EXPECT_TRUE(HasPermission(PrivilegeOrigin::ServerConsole,
                            ToMask(Permission::CommandTeleport), 0));
}

TEST(PermissionsTests, ZeroRequiredAlwaysTrue) {
  EXPECT_TRUE(HasPermission(PrivilegeOrigin::GameClient, 0, 0));
}

TEST(PermissionsTests, GameMasterRoleMaskHasGameplay) {
  PermissionMask const gm = DefaultPermissions(AccessLevel::GameMaster);
  EXPECT_TRUE(HasPermission(PrivilegeOrigin::GameClient,
                            ToMask(Permission::CommandGameplay), gm));
}

TEST(PermissionsTests, CustomRoleMaskGrantsTeleport) {
  PermissionMask const custom = ToMask(Permission::CommandTeleport);
  EXPECT_TRUE(HasPermission(PrivilegeOrigin::GameClient,
                            ToMask(Permission::CommandTeleport), custom));
}

TEST(PermissionsTests, ModeratorDotGate) {
  PermissionMask const mod = DefaultPermissions(AccessLevel::Moderator);
  EXPECT_TRUE(CanUseModeratorDotCommands(mod));
  EXPECT_FALSE(CanUseGameMasterDotCommands(mod));
}

TEST(PermissionsTests, GameMasterDotGate) {
  PermissionMask const gm = DefaultPermissions(AccessLevel::GameMaster);
  EXPECT_TRUE(CanUseGameMasterDotCommands(gm));
}

TEST(PermissionsTests, RealmGateUsesStaffTierRank) {
  PermissionMask const gm = DefaultPermissions(AccessLevel::GameMaster);
  EXPECT_TRUE(MeetsRealmSecurityRequirement(gm, 1));
  EXPECT_TRUE(MeetsRealmSecurityRequirement(gm, 2));
  EXPECT_FALSE(MeetsRealmSecurityRequirement(gm, 3));
}
