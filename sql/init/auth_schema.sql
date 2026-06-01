CREATE DATABASE IF NOT EXISTS `firelands_auth`;
USE `firelands_auth`;

CREATE TABLE IF NOT EXISTS `account` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `username` varchar(32) NOT NULL DEFAULT '',
  `salt` binary(32) NOT NULL DEFAULT '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0',
  `verifier` binary(32) NOT NULL DEFAULT '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0',
  `email` varchar(255) NOT NULL DEFAULT '',
  `joindate` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `last_ip` varchar(15) NOT NULL DEFAULT '127.0.0.1',
  `expansion` tinyint(3) unsigned NOT NULL DEFAULT '3', -- 3 for Cataclysm
  `access_level` tinyint unsigned NOT NULL DEFAULT '0', -- Legacy (migrated to RBAC); kept at 0
  `locked` tinyint unsigned NOT NULL DEFAULT '0', -- 1 = banned (auth rejects login); see migration 16
  PRIMARY KEY (`id`),
  UNIQUE KEY `idx_username` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `realmlist` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(32) NOT NULL DEFAULT '',
  `address` varchar(255) NOT NULL DEFAULT '127.0.0.1',
  `port` smallint(5) unsigned NOT NULL DEFAULT '8085',
  `icon` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `timezone` tinyint(3) unsigned NOT NULL DEFAULT '1',
  -- Minimum `account.access_level` (0–3) required to see/join this realm.
  `allowedSecurityLevel` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `population` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  UNIQUE KEY `idx_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `account_session` (
  `id` int(10) unsigned NOT NULL,
  `session_key` binary(40) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `account_data` (
  `accountId` int(10) unsigned NOT NULL,
  `type` tinyint(3) unsigned NOT NULL,
  `time` int(10) unsigned NOT NULL DEFAULT '0',
  `data` blob NOT NULL,
  PRIMARY KEY (`accountId`, `type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

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
