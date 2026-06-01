USE `firelands_auth`;

CREATE TABLE IF NOT EXISTS `rbac_role` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(64) NOT NULL,
  `permission_mask` bigint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_rbac_role_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `rbac_account_role` (
  `account_id` int unsigned NOT NULL,
  `role_id` int unsigned NOT NULL,
  PRIMARY KEY (`account_id`, `role_id`),
  KEY `idx_rbac_account_role_role` (`role_id`),
  CONSTRAINT `fk_rbac_account_role_account`
    FOREIGN KEY (`account_id`) REFERENCES `account` (`id`) ON DELETE CASCADE,
  CONSTRAINT `fk_rbac_account_role_role`
    FOREIGN KEY (`role_id`) REFERENCES `rbac_role` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
