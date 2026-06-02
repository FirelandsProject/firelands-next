USE `firelands_world`;

DROP TABLE IF EXISTS `firelands_commands`;
CREATE TABLE `firelands_commands` (
  `name` varchar(64) NOT NULL,
  `description` varchar(255) NOT NULL DEFAULT '',
  `syntax` varchar(255) NOT NULL DEFAULT '',
  `required_permission_mask` bigint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Permission mask values match shared/game/Permissions.h
-- 0 = anyone, otherwise must match GetAccountRolePermissionMask()

INSERT INTO `firelands_commands` (`name`, `description`, `syntax`, `required_permission_mask`) VALUES
-- Anyone (mask=0)
('help', 'Show available commands', '.help', 0),
('commands', 'Show available commands (alias)', '.commands', 0),

-- GPS / Position (mask=1 = CommandGps)
('gps', 'Show current position and map', '.gps', 1),
('mmap', 'Navmesh pathfinding info and visual markers', '.mmap [x y z [mapId]] | .mmap clear', 1),

-- Mailbox (mask=512 = CommandMailbox)
('email', 'Open mailbox anywhere', '.email', 512),

-- Teleport (mask=2 = CommandTeleport)
('tele', 'Teleport to coordinates or location name', '.tele <x> <y> <z> [mapId]', 2),

-- GM Tools (mask=64 = CommandGmTools)
('gm', 'Toggle GM mode on/off (NPCs ignore you)', '.gm [on|off]', 64),
('dnd', 'Toggle Do Not Disturb tag', '.dnd [on|off]', 64),
('dev', 'Toggle Developer tag', '.dev [on|off]', 64),
('visible', 'Toggle GM visibility to players', '.visible [on|off]', 64),
('fly', 'Toggle fly mode', '.fly [on|off]', 64),
('speed', 'Set movement speed multiplier', '.speed <value>  (1=normal)', 64),

-- Manage Players (mask=8)
('online', 'List online players', '.online', 8),
('announce', 'Send server-wide announcement', '.announce <message>', 8),
('kick', 'Kick a player from the server', '.kick <playerName> [reason]', 8),
('goto', 'Teleport to a player', '.goto <playerName>', 8),
('appear', 'Teleport to a player (alias)', '.appear <playerName>', 8),
('summon', 'Summon a player to your location', '.summon <playerName>', 8),

-- Gameplay (mask=128 = CommandGameplay)
('learn', 'Learn a spell by ID', '.learn <spellId> [all]', 128),
('unlearn', 'Unlearn a spell by ID', '.unlearn <spellId> [all]', 128),
('money', 'Modify money (copper)', '.money <copper>', 128),
('additem', 'Add item to inventory', '.additem <itemId> [count]', 128),
('delitem', 'Delete item from inventory', '.delitem <itemId> [count]', 128),
('level', 'Set character level', '.level <value>', 128),
('cd', 'Reset all spell cooldowns', '.cd', 128),
('damage', 'Deal damage to targeted creature', '.damage <amount>', 128),
('revive', 'Revive yourself or targeted player', '.revive', 128),
('faction', 'Change faction reaction', '.faction <factionId> <reaction>', 128),

-- GM Tickets (mask=256 = ManageGmTickets)
('ticket', 'Manage GM tickets', '.ticket list|close|respond', 256),

-- Accounts (mask=16 = ManageAccounts)
('account', 'Manage accounts (console only)', '.account create|delete|setaccess', 16),
('ban', 'Ban an account (console only)', '.ban <accountName> [reason]', 16),
('unban', 'Unban an account (console only)', '.unban <accountName>', 16),
('rbac', 'RBAC role management (console only)', '.rbac setstaff|grant|revoke|show', 16),

-- Server Control (mask=32)
('server', 'Server control commands', '.server shutdown|restart <seconds>', 32),
('npc', 'NPC management', '.npc spawn|delete|info <entry>', 32);
