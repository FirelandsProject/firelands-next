-- Rested XP pool (Cataclysm `PLAYER_REST_STATE_EXPERIENCE` / rest indicator).
USE `firelands_characters`;

ALTER TABLE `characters`
  ADD COLUMN `rest_bonus` float NOT NULL DEFAULT 0 AFTER `xp`;
