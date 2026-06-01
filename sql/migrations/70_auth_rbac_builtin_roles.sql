USE `firelands_auth`;

-- Masks match `DefaultPermissions()` in shared/game/Permissions.h (4.3.4 staff set).
INSERT IGNORE INTO `rbac_role` (`name`, `permission_mask`) VALUES
  ('moderator', 517),
  ('gamemaster', 975),
  ('administrator', 1023);

INSERT IGNORE INTO `rbac_account_role` (`account_id`, `role_id`)
SELECT a.id, r.id
FROM `account` a
INNER JOIN `rbac_role` r ON r.name = CASE a.access_level
  WHEN 1 THEN 'moderator'
  WHEN 2 THEN 'gamemaster'
  WHEN 3 THEN 'administrator'
  ELSE ''
END
WHERE a.access_level > 0;

UPDATE `account` SET `access_level` = 0 WHERE `access_level` > 0;
