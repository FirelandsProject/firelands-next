-- Repair / seed `player_classlevelstats` if migration `17_*.sql` was skipped or DB was reset.
USE `firelands_world`;

INSERT IGNORE INTO `player_classlevelstats`
  (`class`, `level`, `str`, `agi`, `sta`, `inte`, `spi`) VALUES
(1,1,23,20,22,20,21),
(2,1,23,20,22,20,22),
(3,1,22,21,22,20,21),
(4,1,23,21,21,20,21),
(5,1,17,22,22,22,23),
(6,1,25,19,22,20,22),
(7,1,22,21,22,20,22),
(8,1,17,22,22,23,23),
(9,1,21,21,22,23,23),
(11,1,22,20,22,22,23);

INSERT IGNORE INTO `player_racestats`
  (`race`, `str`, `agi`, `sta`, `inte`, `spi`) VALUES
(1,0,0,0,0,0),(2,3,-3,3,-3,0),(3,0,0,1,0,0),(4,-4,2,0,0,0),
(5,0,0,0,0,0),(6,1,0,1,0,0),(7,-5,2,0,3,0),(8,1,2,0,0,0),
(9,0,0,0,0,0),(10,0,0,0,0,0),(11,0,0,0,2,0),(22,0,0,0,0,0);
