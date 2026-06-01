-- Quest chain prerequisite (`quest_template_addon.PrevQuestID` in ref).
USE `firelands_world`;

SET @exist_prev_quest_id :=
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
   WHERE TABLE_SCHEMA = DATABASE()
     AND TABLE_NAME = 'quest_template'
     AND COLUMN_NAME = 'PrevQuestId');

SET @fl_sql := IF(@exist_prev_quest_id = 0,
  'ALTER TABLE `quest_template`
     ADD COLUMN `PrevQuestId` int NOT NULL DEFAULT ''0'' COMMENT ''Prev quest: >0 rewarded, <0 active''',
  'SELECT 1');

PREPARE _fl_m67_prev FROM @fl_sql;
EXECUTE _fl_m67_prev;
DEALLOCATE PREPARE _fl_m67_prev;
