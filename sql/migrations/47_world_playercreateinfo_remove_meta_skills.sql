-- Remove meta / profession / riding starter skills that confuse the client skill UI.
USE `firelands_world`;

DELETE FROM `playercreateinfo_skill` WHERE `skillId` IN (
  183,  -- GENERIC (DND)
  777,  -- Mounts
  778,  -- Companion Pets
  810,  -- Glyphs
  762,  -- Riding
  129,  -- First Aid
  171, 164, 165, 182, 185, 186, 197, 202, 333, 356, 393, 773
);

DELETE FROM `playercreateinfo_skill` WHERE `skillId` IN (
  6, 8, 38, 39, 50, 51, 56, 78, 134, 163, 184, 237, 253, 267,
  354, 355, 373, 374, 375, 573, 574, 593, 594, 613,
  770, 771, 772, 795, 796, 797, 798, 799, 800, 801, 802, 803, 804
);
