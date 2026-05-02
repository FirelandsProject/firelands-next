-- Passive "Language *" spells per race (class=0 wildcard), 4.3.4 spell IDs.
-- Aligns with `shared/game/ChatLanguages.cpp` / Trinity `playercreateinfo_spell_custom`.
-- `(race=0,class=0)` already seeds Common (668) and Orcish (669) for all templates;
-- these rows add racial languages so `GetStarterSpells` matches what the client
-- expects for `/say` language dropdowns.

INSERT IGNORE INTO `firelands_world`.`playercreateinfo_spell` (`race`, `class`, `spellId`) VALUES
  (1, 0, 668),
  (2, 0, 669),
  (3, 0, 672),
  (3, 0, 668),
  (4, 0, 671),
  (4, 0, 668),
  (5, 0, 17737),
  (5, 0, 669),
  (6, 0, 670),
  (6, 0, 669),
  (7, 0, 7340),
  (7, 0, 668),
  (8, 0, 7341),
  (8, 0, 669),
  (9, 0, 69269),
  (9, 0, 669),
  (10, 0, 813),
  (10, 0, 669),
  (11, 0, 29932),
  (11, 0, 668),
  (22, 0, 69270),
  (22, 0, 668);
