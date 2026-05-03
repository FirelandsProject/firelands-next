-- Troll starter rows were inserted with map=0 (Eastern Kingdoms) but Kalimdor
-- coordinates, causing a void fall on first login. Echo Isles (map 1, zone 5691)
-- matches CharacterService FallbackStartPosition for race 8. DK (class 6) unchanged.

UPDATE `firelands_world`.`playercreateinfo`
SET `map` = 1,
    `zone` = 5691,
    `position_x` = -1171.45,
    `position_y` = -5263.65,
    `position_z` = 0.847728,
    `orientation` = 5.78945
WHERE `race` = 8 AND `class` IN (1, 2, 3, 4, 5, 7, 8, 9);
