CREATE DATABASE IF NOT EXISTS `firelands_auth`;
USE `firelands_auth`;

CREATE TABLE IF NOT EXISTS `account` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `username` varchar(32) NOT NULL DEFAULT '',
  `sha_pass_hash` varchar(40) NOT NULL DEFAULT '',
  `email` varchar(255) NOT NULL DEFAULT '',
  `joindate` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `last_ip` varchar(15) NOT NULL DEFAULT '127.0.0.1',
  `expansion` tinyint(3) unsigned NOT NULL DEFAULT '3', -- 3 for Cataclysm
  PRIMARY KEY (`id`),
  UNIQUE KEY `idx_username` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Seed a test account (admin:admin) 
-- SHA1('ADMIN:ADMIN') = 40b244112641dd78dd4f93b658a731844fd12739
INSERT INTO `account` (`id`, `username`, `sha_pass_hash`, `email`, `expansion`) 
VALUES (1, 'ADMIN', '40B244112641DD78DD4F93B658A731844FD12739', 'admin@firelands.com', 3);
