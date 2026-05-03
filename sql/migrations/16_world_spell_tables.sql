-- Spell-related world tables (Cataclysm 4.3.4 / TCPP parity).
-- Source: The-Cataclysm-Preservation-Project/TrinityCore sql/base/dev/world_database.sql
-- (spell_area through spelleffect_dbc). Empty at install; gameplay can INSERT as needed.
CREATE DATABASE IF NOT EXISTS `firelands_world`;
USE `firelands_world`;

CREATE TABLE IF NOT EXISTS `spell_area` (
  `spell` int unsigned NOT NULL DEFAULT 0,
  `area` int unsigned NOT NULL DEFAULT 0,
  `quest_start` int unsigned NOT NULL DEFAULT 0,
  `quest_end` int unsigned NOT NULL DEFAULT 0,
  `aura_spell` int NOT NULL DEFAULT 0,
  `racemask` int unsigned NOT NULL DEFAULT 0,
  `gender` tinyint unsigned NOT NULL DEFAULT 2,
  `flags` tinyint unsigned NOT NULL DEFAULT 3,
  `quest_start_status` int NOT NULL DEFAULT 64,
  `quest_end_status` int NOT NULL DEFAULT 11,
  PRIMARY KEY (`spell`,`area`,`quest_start`,`aura_spell`,`racemask`,`gender`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spell_bonus_data` (
  `entry` int unsigned NOT NULL DEFAULT 0,
  `direct_bonus` float NOT NULL DEFAULT 0,
  `dot_bonus` float NOT NULL DEFAULT 0,
  `ap_bonus` float NOT NULL DEFAULT 0,
  `ap_dot_bonus` float NOT NULL DEFAULT 0,
  `comments` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spell_custom_attr` (
  `entry` int unsigned NOT NULL DEFAULT 0 COMMENT 'spell id',
  `attributes` int unsigned NOT NULL DEFAULT 0 COMMENT 'SpellCustomAttributes',
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='SpellInfo custom attributes';

CREATE TABLE IF NOT EXISTS `spell_dbc` (
  `Id` int unsigned NOT NULL,
  `Attributes` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx2` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx3` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx4` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx5` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx6` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx7` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx8` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx9` int unsigned NOT NULL DEFAULT 0,
  `AttributesEx10` int unsigned NOT NULL DEFAULT 0,
  `CastingTimeIndex` int unsigned NOT NULL DEFAULT 1,
  `DurationIndex` int unsigned NOT NULL DEFAULT 0,
  `RangeIndex` int unsigned NOT NULL DEFAULT 1,
  `SchoolMask` int unsigned NOT NULL DEFAULT 0,
  `SpellAuraOptionsId` int unsigned NOT NULL DEFAULT 0,
  `SpellCastingRequirementsId` int unsigned NOT NULL DEFAULT 0,
  `SpellCategoriesId` int unsigned NOT NULL DEFAULT 0,
  `SpellClassOptionsId` int unsigned NOT NULL DEFAULT 0,
  `SpellEquippedItemsId` int unsigned NOT NULL DEFAULT 0,
  `SpellInterruptsId` int unsigned NOT NULL DEFAULT 0,
  `SpellLevelsId` int unsigned NOT NULL DEFAULT 0,
  `SpellTargetRestrictionsId` int unsigned NOT NULL DEFAULT 0,
  `PowerType` int unsigned DEFAULT NULL COMMENT 'Optional power type override, NULL uses DBC',
  `OvAttributes` int unsigned DEFAULT NULL COMMENT 'Override SpellDefinition.attributes, NULL keeps DBC',
  `OvCastingTimeIndex` int unsigned DEFAULT NULL COMMENT 'Override castingTimeIndex, NULL keeps DBC',
  `OvDurationIndex` int unsigned DEFAULT NULL COMMENT 'Override durationIndex, NULL keeps DBC',
  `OvRangeIndex` int unsigned DEFAULT NULL COMMENT 'Override rangeIndex, NULL keeps DBC',
  `OvSchoolMask` int unsigned DEFAULT NULL COMMENT 'Override schoolMask, NULL keeps DBC',
  `MvpDirectHealthDelta` int DEFAULT NULL COMMENT 'Phase D MVP: direct health delta on hit, NULL = none',
  `SpellName` varchar(128) NOT NULL,
  PRIMARY KEY (`Id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Custom spell.dbc entries';

CREATE TABLE IF NOT EXISTS `spell_enchant_proc_data` (
  `entry` int unsigned NOT NULL,
  `customChance` int unsigned NOT NULL DEFAULT 0,
  `PPMChance` float NOT NULL DEFAULT 0,
  `procEx` int unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Spell enchant proc data';

CREATE TABLE IF NOT EXISTS `spell_group` (
  `id` int unsigned NOT NULL DEFAULT 0,
  `spell_id` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`,`spell_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Spell System';

CREATE TABLE IF NOT EXISTS `spell_group_stack_rules` (
  `group_id` int unsigned NOT NULL DEFAULT 0,
  `stack_rule` tinyint NOT NULL DEFAULT 0,
  PRIMARY KEY (`group_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spell_learn_spell` (
  `entry` int unsigned NOT NULL DEFAULT 0,
  `SpellID` int unsigned NOT NULL DEFAULT 0,
  `Active` tinyint unsigned NOT NULL DEFAULT 1,
  PRIMARY KEY (`entry`,`SpellID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Item System';

CREATE TABLE IF NOT EXISTS `spell_linked_spell` (
  `spell_trigger` int NOT NULL,
  `spell_effect` int NOT NULL DEFAULT 0,
  `type` tinyint unsigned NOT NULL DEFAULT 0,
  `comment` text NOT NULL,
  UNIQUE KEY `trigger_effect_type` (`spell_trigger`,`spell_effect`,`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Spell System';

CREATE TABLE IF NOT EXISTS `spell_loot_template` (
  `Entry` int unsigned NOT NULL DEFAULT 0,
  `Item` int unsigned NOT NULL DEFAULT 0,
  `Reference` int unsigned NOT NULL DEFAULT 0,
  `Chance` float NOT NULL DEFAULT 100,
  `QuestRequired` tinyint NOT NULL DEFAULT 0,
  `IsCurrency` tinyint NOT NULL DEFAULT 0,
  `LootMode` smallint unsigned NOT NULL DEFAULT 1,
  `GroupId` tinyint unsigned NOT NULL DEFAULT 0,
  `MinCount` tinyint unsigned NOT NULL DEFAULT 1,
  `MaxCount` tinyint unsigned NOT NULL DEFAULT 1,
  `Comment` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`Entry`,`Item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Loot System';

CREATE TABLE IF NOT EXISTS `spell_pet_auras` (
  `spell` int unsigned NOT NULL COMMENT 'dummy spell id',
  `effectId` tinyint unsigned NOT NULL DEFAULT 0,
  `pet` int unsigned NOT NULL DEFAULT 0 COMMENT 'pet id; 0 = all',
  `aura` int unsigned NOT NULL COMMENT 'pet aura id',
  PRIMARY KEY (`spell`,`effectId`,`pet`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spell_proc` (
  `SpellId` int NOT NULL DEFAULT 0,
  `SchoolMask` tinyint unsigned NOT NULL DEFAULT 0,
  `SpellFamilyName` smallint unsigned NOT NULL DEFAULT 0,
  `SpellFamilyMask0` int unsigned NOT NULL DEFAULT 0,
  `SpellFamilyMask1` int unsigned NOT NULL DEFAULT 0,
  `SpellFamilyMask2` int unsigned NOT NULL DEFAULT 0,
  `ProcFlags` int unsigned NOT NULL DEFAULT 0,
  `SpellTypeMask` int unsigned NOT NULL DEFAULT 0,
  `SpellPhaseMask` int unsigned NOT NULL DEFAULT 0,
  `HitMask` int unsigned NOT NULL DEFAULT 0,
  `AttributesMask` int unsigned NOT NULL DEFAULT 0,
  `DisableEffectsMask` int unsigned NOT NULL DEFAULT 0,
  `ProcsPerMinute` float NOT NULL DEFAULT 0,
  `Chance` float NOT NULL DEFAULT 0,
  `Cooldown` int unsigned NOT NULL DEFAULT 0,
  `Charges` tinyint unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`SpellId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spell_proc_event` (
  `entry` int NOT NULL DEFAULT 0,
  `SchoolMask` tinyint NOT NULL DEFAULT 0,
  `SpellFamilyName` smallint unsigned NOT NULL DEFAULT 0,
  `SpellFamilyMask0` int unsigned NOT NULL DEFAULT 0,
  `SpellFamilyMask1` int unsigned NOT NULL DEFAULT 0,
  `SpellFamilyMask2` int unsigned NOT NULL DEFAULT 0,
  `procFlags` int unsigned NOT NULL DEFAULT 0,
  `procEx` int unsigned NOT NULL DEFAULT 0,
  `ppmRate` float NOT NULL DEFAULT 0,
  `CustomChance` float NOT NULL DEFAULT 0,
  `Cooldown` int unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spell_ranks` (
  `first_spell_id` int unsigned NOT NULL DEFAULT 0,
  `spell_id` int unsigned NOT NULL DEFAULT 0,
  `rank` tinyint unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`first_spell_id`,`rank`),
  UNIQUE KEY `spell_id` (`spell_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Spell Rank Data';

CREATE TABLE IF NOT EXISTS `spell_required` (
  `spell_id` int NOT NULL DEFAULT 0,
  `req_spell` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`spell_id`,`req_spell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Spell Additinal Data';

CREATE TABLE IF NOT EXISTS `spell_script_names` (
  `spell_id` int NOT NULL,
  `ScriptName` varchar(64) NOT NULL,
  UNIQUE KEY `spell_id` (`spell_id`,`ScriptName`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spell_scripts` (
  `id` int unsigned NOT NULL DEFAULT 0,
  `effIndex` tinyint unsigned NOT NULL DEFAULT 0,
  `delay` int unsigned NOT NULL DEFAULT 0,
  `command` int unsigned NOT NULL DEFAULT 0,
  `datalong` int unsigned NOT NULL DEFAULT 0,
  `datalong2` int unsigned NOT NULL DEFAULT 0,
  `dataint` int NOT NULL DEFAULT 0,
  `x` float NOT NULL DEFAULT 0,
  `y` float NOT NULL DEFAULT 0,
  `z` float NOT NULL DEFAULT 0,
  `o` float NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spell_target_position` (
  `ID` int unsigned NOT NULL DEFAULT 0 COMMENT 'Identifier',
  `EffectIndex` tinyint unsigned NOT NULL DEFAULT 0,
  `MapID` smallint unsigned NOT NULL DEFAULT 0,
  `PositionX` float NOT NULL DEFAULT 0,
  `PositionY` float NOT NULL DEFAULT 0,
  `PositionZ` float NOT NULL DEFAULT 0,
  `Orientation` float NOT NULL DEFAULT 0,
  `VerifiedBuild` smallint DEFAULT 0,
  PRIMARY KEY (`ID`,`EffectIndex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Spell System';

CREATE TABLE IF NOT EXISTS `spell_threat` (
  `entry` int unsigned NOT NULL,
  `flatMod` int DEFAULT NULL,
  `pctMod` float NOT NULL DEFAULT 1 COMMENT 'threat multiplier for damage/healing',
  `apPctMod` float NOT NULL DEFAULT 0 COMMENT 'additional threat bonus from attack power',
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spelldifficulty_dbc` (
  `id` int unsigned NOT NULL DEFAULT 0,
  `spellid0` int unsigned NOT NULL DEFAULT 0,
  `spellid1` int unsigned NOT NULL DEFAULT 0,
  `spellid2` int unsigned NOT NULL DEFAULT 0,
  `spellid3` int unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `spelleffect_dbc` (
  `Id` int unsigned NOT NULL,
  `Effect` int unsigned NOT NULL DEFAULT 0,
  `EffectAmplitude` float NOT NULL DEFAULT 0,
  `EffectAura` int unsigned NOT NULL DEFAULT 0,
  `EffectAuraPeriod` int unsigned NOT NULL DEFAULT 0,
  `EffectBasePoints` int NOT NULL DEFAULT 0,
  `EffectBonusCoefficient` float NOT NULL DEFAULT 0,
  `EffectChainAmplitude` float NOT NULL DEFAULT 0,
  `EffectChainTargets` int unsigned NOT NULL DEFAULT 0,
  `EffectDieSides` int NOT NULL DEFAULT 0,
  `EffectItemType` int unsigned NOT NULL DEFAULT 0,
  `EffectMechanic` int unsigned NOT NULL DEFAULT 0,
  `EffectMiscValue` int NOT NULL DEFAULT 0,
  `EffectMiscValueB` int NOT NULL DEFAULT 0,
  `EffectPointsPerResource` float NOT NULL DEFAULT 0,
  `EffectRadiusIndex` int unsigned NOT NULL DEFAULT 0,
  `EffectRadiusMaxIndex` int unsigned NOT NULL DEFAULT 0,
  `EffectRealPointsPerLevel` float NOT NULL DEFAULT 0,
  `EffectSpellClassMaskA` int unsigned NOT NULL DEFAULT 0,
  `EffectSpellClassMaskB` int unsigned NOT NULL DEFAULT 0,
  `EffectSpellClassMaskC` int unsigned NOT NULL DEFAULT 0,
  `EffectTriggerSpell` int unsigned NOT NULL DEFAULT 0,
  `EffectImplicitTargetA` int unsigned NOT NULL DEFAULT 0,
  `EffectImplicitTargetB` int unsigned NOT NULL DEFAULT 0,
  `SpellID` int unsigned NOT NULL,
  `EffectIndex` int unsigned NOT NULL DEFAULT 0,
  `Comment` varchar(128) NOT NULL,
  PRIMARY KEY (`Id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
