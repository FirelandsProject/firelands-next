USE `firelands_world`;

DROP TABLE IF EXISTS `firelands_commands`;
CREATE TABLE `firelands_commands` (
  `name` varchar(64) NOT NULL,
  `description` varchar(255) NOT NULL DEFAULT '',
  `syntax` varchar(255) NOT NULL DEFAULT '',
  `min_access_level` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO `firelands_commands` (`name`, `description`, `syntax`, `min_access_level`) VALUES
-- Player (0)
('help', 'Show available commands', '.help', 0),
('commands', 'Show available commands (alias)', '.commands', 0),

-- Moderator (1)
('gps', 'Show current position and map', '.gps', 1),
('mmap', 'Navmesh pathfinding info and visual markers', '.mmap [x y z [mapId]] | .mmap clear', 1),
('email', 'Open mailbox anywhere', '.email', 1),
('online', 'List online players', '.online', 1),
('announce', 'Send server-wide message', '.announce <message>', 1),
('kick', 'Kick a player from the server', '.kick <playerName> [reason]', 1),
('goto', 'Teleport to a player', '.goto <playerName>', 1),
('appear', 'Teleport to a player (alias)', '.appear <playerName>', 1),

-- Game Master (2)
('tele', 'Teleport to coordinates', '.tele <x> <y> <z> [mapId]', 2),
('gm', 'Toggle GM mode (NPCs ignore you)', '.gm [on|off]', 2),
('dnd', 'Toggle Do Not Disturb tag', '.dnd [on|off]', 2),
('dev', 'Toggle Developer tag', '.dev [on|off]', 2),
('visible', 'Toggle GM visibility to players', '.visible [on|off]', 2),
('fly', 'Toggle fly mode', '.fly [on|off]', 2),
('speed', 'Set movement speed multiplier', '.speed <value>  (1=normal, 2=double, 0.5=half)', 2),
('summon', 'Summon a player to your location', '.summon <playerName>', 2),
('learn', 'Learn a spell', '.learn <spellId> [all]', 2),
('unlearn', 'Unlearn a spell', '.unlearn <spellId> [all]', 2),
('money', 'Modify money', '.money <copper>', 2),
('additem', 'Add item to inventory', '.additem <itemId> [count]', 2),
('delitem', 'Delete item from inventory', '.delitem <itemId> [count]', 2),
('level', 'Set character level', '.level <value>', 2),
('cd', 'Reset spell cooldowns', '.cd', 2),
('damage', 'Deal damage to targeted creature', '.damage <amount>', 2),
('revive', 'Revive yourself or targeted player', '.revive', 2),
('ticket', 'Manage GM tickets', '.ticket list | .ticket close <id> | .ticket respond <id> <msg>', 2),
('faction', 'Change faction reaction', '.faction <factionId> <reaction>', 2),

-- Administrator (3)
('account', 'Manage accounts (console only)', '.account create|delete|setaccess <args>', 3),
('ban', 'Ban an account (console only)', '.ban <accountName> [reason]', 3),
('unban', 'Unban an account (console only)', '.unban <accountName>', 3),
('server', 'Server control commands', '.server shutdown|restart <seconds>', 3),
('npc', 'NPC management commands', '.npc spawn|delete <entry>', 3),
('rbac', 'RBAC role management (console only)', '.rbac', 3);
