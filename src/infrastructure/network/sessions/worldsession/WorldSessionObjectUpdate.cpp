#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <application/combat/PlayerCombatStats.h>
#include <application/world/PlayerQuestProgressStore.h>
#include <domain/models/PlayerCreateInfo.h>
#include <shared/game/PlayerQuestLog.h>
#include <shared/network/UpdateData.h>
#include <shared/network/UpdateFields.h>
#include <shared/game/ChatLanguages.h>
#include <shared/game/EquipmentCache.h>
#include <shared/dbc/GtPlayerStatGameTables.h>
#include <shared/game/PlayerClass.h>
#include <shared/game/StatFormulas.h>
#include <shared/game/UnitCombatStats.h>
#include <shared/game/UnitFieldFlags.h>
#include <shared/game/InventorySlots.h>
#include <shared/game/WowGuid.h>
#include <shared/game/RestExperienceLogic.h>
#include <shared/network/MovementStateQueries.h>
#include <shared/network/WorldOpcodes.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <optional>
#include <utility>

namespace Firelands {
namespace WorldSessionObjectUpdate {

namespace {

void SetPackedShort(std::map<uint16, uint32> &fields, uint16 field, uint8 slot,
                    uint16 value) {
  uint32 &packed = fields[static_cast<uint16>(field + (slot / 2))];
  if ((slot % 2) == 0)
    packed = (packed & 0xFFFF0000u) | value;
  else
    packed = (packed & 0x0000FFFFu) | (static_cast<uint32>(value) << 16);
}

/// Cataclysm 4.3.4: each skill slot uses its own update-field index (Size 64
/// TWO_SHORT arrays are 64 entries, not 32 packed pairs).
void SetPlayerFieldByte(std::map<uint16, uint32> &fields, uint16 field, uint8 offset,
                        uint8 value) {
  uint32 &packed = fields[field];
  uint8 bytes[4]{};
  std::memcpy(bytes, &packed, sizeof(bytes));
  if (offset < 4)
    bytes[offset] = value;
  std::memcpy(&packed, bytes, sizeof(bytes));
}

void ApplyRestExperienceWireFields(std::map<uint16, uint32> &fields, float restBonus,
                                   uint8_t facialHair) {
  fields[PLAYER_REST_STATE_EXPERIENCE] =
      RestExperienceLogic::RestBonusWireAmount(restBonus);
  SetPlayerFieldByte(fields, PLAYER_BYTES_2, 0, facialHair);
  SetPlayerFieldByte(
      fields, PLAYER_BYTES_2, RestExperienceLogic::kPlayerBytes2OffsetRestState,
      static_cast<uint8_t>(RestExperienceLogic::RestStateForBonus(restBonus)));
}

void SetPlayerSkillSlot(std::map<uint16, uint32> &fields, uint16 baseField,
                        uint8 slot, uint16 value) {
  if (slot >= 64)
    return;
  fields[static_cast<uint16>(baseField + slot)] =
      static_cast<uint32>(value) & 0xFFFFu;
}

void AddPlayerSkillFields(std::map<uint16, uint32> &fields,
                          std::vector<StarterSkillGrant> const &skills) {
  size_t const used = std::min(skills.size(), size_t{64});
  for (size_t i = 0; i < used; ++i) {
    uint8 const slot = static_cast<uint8>(i);
    uint16 const skill = static_cast<uint16>(skills[i].skillId);
    uint16 const rank = skills[i].rank > 0 ? skills[i].rank : 1;
    uint16 const maxRank =
        skills[i].maxRank > 0 ? skills[i].maxRank : rank;
    SetPlayerSkillSlot(fields, PLAYER_SKILL_LINEID_0, slot, skill);
    SetPlayerSkillSlot(fields, PLAYER_SKILL_STEP_0, slot, 0);
    SetPlayerSkillSlot(fields, PLAYER_SKILL_RANK_0, slot, rank);
    SetPlayerSkillSlot(fields, PLAYER_SKILL_MAX_RANK_0, slot, maxRank);
    SetPlayerSkillSlot(fields, PLAYER_SKILL_MODIFIER_0, slot, 0);
    SetPlayerSkillSlot(fields, PLAYER_SKILL_TALENT_0, slot, 0);
  }
  for (size_t i = used; i < 64; ++i) {
    uint8 const slot = static_cast<uint8>(i);
    SetPlayerSkillSlot(fields, PLAYER_SKILL_LINEID_0, slot, 0);
    SetPlayerSkillSlot(fields, PLAYER_SKILL_STEP_0, slot, 0);
    SetPlayerSkillSlot(fields, PLAYER_SKILL_RANK_0, slot, 0);
    SetPlayerSkillSlot(fields, PLAYER_SKILL_MAX_RANK_0, slot, 0);
    SetPlayerSkillSlot(fields, PLAYER_SKILL_MODIFIER_0, slot, 0);
    SetPlayerSkillSlot(fields, PLAYER_SKILL_TALENT_0, slot, 0);
  }
}

uint32 PackFloat(float value) {
  uint32 bits = 0;
  static_assert(sizeof(bits) == sizeof(value), "float packing");
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

void SetBaselinePercentMultipliers(std::map<uint16, uint32> &fields, uint16 baseField,
                                   uint32 count) {
  uint32 const one = PackFloat(1.0f);
  for (uint32 i = 0; i < count; ++i)
    fields[static_cast<uint16>(baseField + i)] = one;
}

bool UsesBaselineHealingPower(uint8 classId) {
  switch (ToPlayerClass(classId)) {
  case PlayerClass::Paladin:
  case PlayerClass::Priest:
  case PlayerClass::Shaman:
  case PlayerClass::Druid:
    return true;
  default:
    return false;
  }
}

void AddBaselineMeleeFields(std::map<uint16, uint32> &fields,
                            Character const &character) {
  int32 const ap = ComputeBaseMeleeAttackPower(character);
  constexpr uint32 kAttackTimeMs = 2000u; // 2.0s baseline weapon speed
  float const speedSec = static_cast<float>(kAttackTimeMs) / 1000.0f;
  float const dpsFromAp = static_cast<float>(ap) / 14.0f;
  float const minDmg = 1.0f + (dpsFromAp * speedSec * 0.85f);
  float const maxDmg = 2.0f + (dpsFromAp * speedSec * 1.15f);

  fields[UNIT_FIELD_BASEATTACKTIME] = kAttackTimeMs;
  fields[static_cast<uint16>(UNIT_FIELD_BASEATTACKTIME + 1)] = kAttackTimeMs;
  fields[UNIT_FIELD_ATTACK_POWER] = static_cast<uint32>(ap);
  fields[UNIT_FIELD_ATTACK_POWER_MOD_POS] = 0;
  fields[UNIT_FIELD_ATTACK_POWER_MOD_NEG] = 0;
  fields[UNIT_FIELD_ATTACK_POWER_MULTIPLIER] = PackFloat(0.0f);
  fields[UNIT_FIELD_MINDAMAGE] = PackFloat(minDmg);
  fields[UNIT_FIELD_MAXDAMAGE] = PackFloat(maxDmg);
  fields[UNIT_FIELD_MINOFFHANDDAMAGE] = PackFloat(0.0f);
  fields[UNIT_FIELD_MAXOFFHANDDAMAGE] = PackFloat(0.0f);
  fields[UNIT_FIELD_RANGED_ATTACK_POWER] = 0;
  fields[UNIT_FIELD_RANGED_ATTACK_POWER_MOD_POS] = 0;
  fields[UNIT_FIELD_RANGED_ATTACK_POWER_MOD_NEG] = 0;
  fields[UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER] = PackFloat(0.0f);
  fields[UNIT_FIELD_MINRANGEDDAMAGE] = PackFloat(0.0f);
  fields[UNIT_FIELD_MAXRANGEDDAMAGE] = PackFloat(0.0f);
  SetBaselinePercentMultipliers(fields, UNIT_FIELD_POWER_COST_MULTIPLIER, 7);
}

void AddBaselineSpellFields(std::map<uint16, uint32> &fields,
                            Character const &character,
                            GtPlayerStatGameTables const *statGameTables) {
  uint8 const klass = ToClassId(character.GetClass());
  uint8 const level = character.GetLevel();
  uint32 const inte = character.GetPrimaryStat(3);
  uint32 const spi = character.GetPrimaryStat(4);

  // Ref `Player::InitStatsForLevel` (cata): cast/haste multipliers start at 1.0 (not 0).
  fields[UNIT_MOD_CAST_SPEED] = PackFloat(1.0f);
  fields[UNIT_MOD_CAST_HASTE] = PackFloat(1.0f);

  fields[PLAYER_FIELD_MOD_HASTE] = PackFloat(1.0f);
  fields[PLAYER_FIELD_MOD_RANGED_HASTE] = PackFloat(1.0f);
  fields[PLAYER_FIELD_MOD_PET_HASTE] = PackFloat(1.0f);
  fields[PLAYER_FIELD_MOD_HASTE_REGEN] = PackFloat(1.0f);

  fields[PLAYER_FIELD_UI_SPELL_HIT_MODIFIER] = PackFloat(
      StatFormulas::SpellHitPercentFromRating(level, 0, statGameTables));

  fields[PLAYER_FIELD_MOD_SPELL_POWER_PCT] = PackFloat(1.0f);
  fields[PLAYER_FIELD_OVERRIDE_SPELL_POWER_BY_AP_PCT] = PackFloat(0.0f);

  for (uint32_t i = 0; i < 7; ++i) {
    fields[static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i)] = 0;
    fields[static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + i)] = 0;
  }
  // Wire type is float in 4.3.4 client; 0.0 displays as 0% and breaks derived spell stats (#inf).
  SetBaselinePercentMultipliers(fields, PLAYER_FIELD_MOD_DAMAGE_DONE_PCT, 7);

  uint32 spellPower = 0;
  if (UsesBaselineSpellPowerFromIntellect(klass))
    spellPower = inte;
  // Schools 1..6 = Holy .. Arcane (index 0 = physical spell power, usually 0).
  for (uint32_t school = 1; school <= 6; ++school)
    fields[static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + school)] =
        spellPower;

  fields[PLAYER_FIELD_MOD_HEALING_DONE_POS] = 0;
  if (UsesBaselineHealingPower(klass)) {
    uint32 const heal = inte + spi / 4u;
    fields[PLAYER_FIELD_MOD_HEALING_DONE_POS] = heal;
  }
  fields[PLAYER_FIELD_MOD_HEALING_PCT] = PackFloat(1.0f);
  fields[PLAYER_FIELD_MOD_HEALING_DONE_PCT] = PackFloat(1.0f);

  fields[static_cast<uint16>(PLAYER_FIELD_WEAPON_DMG_MULTIPLIERS + 0)] =
      PackFloat(1.0f);
  fields[static_cast<uint16>(PLAYER_FIELD_WEAPON_DMG_MULTIPLIERS + 1)] =
      PackFloat(1.0f);
  fields[static_cast<uint16>(PLAYER_FIELD_WEAPON_DMG_MULTIPLIERS + 2)] =
      PackFloat(1.0f);

  // Spell crit % per school (client expects percent points, e.g. 5.0 = 5%).
  // TODO: school-specific modifiers; physical (s==0) stays 0.
  uint32 const critSpellRating = 0;
  float const spellCritPct = StatFormulas::SpellCritPercent(
      level, klass, inte, critSpellRating, statGameTables);
  fields[PLAYER_SPELL_CRIT_PERCENTAGE1] = PackFloat(0.0f);
  for (uint32_t s = 1; s < 7; ++s) {
    fields[static_cast<uint16>(PLAYER_SPELL_CRIT_PERCENTAGE1 + s)] =
        PackFloat(spellCritPct);
  }

  fields[PLAYER_PET_SPELL_POWER] = 0;
}

void AddBaselineDefenseAndResistanceFields(
    std::map<uint16, uint32> &fields, Character const &character,
    GtPlayerStatGameTables const *statGameTables) {
  uint8 const klass = ToClassId(character.GetClass());
  uint8 const level = character.GetLevel();
  uint32 const agi = character.GetPrimaryStat(1);
  uint32 const str = character.GetPrimaryStat(0);
  uint32 const sta = character.GetPrimaryStat(2);
  uint32 const spi = character.GetPrimaryStat(4);
  uint32 const lv = static_cast<uint32>(level);

  uint32 const armor = ComputeBaselineArmor(klass, level, agi, str, sta);
  fields[UNIT_FIELD_RESISTANCES] = armor;
  uint32 const mr = lv / 2u + spi / 6u;
  for (uint32_t school = 1; school <= 6; ++school)
    fields[static_cast<uint16>(UNIT_FIELD_RESISTANCES + school)] = mr;

  for (uint32_t i = 0; i < 7; ++i) {
    fields[static_cast<uint16>(UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE + i)] = 0;
    fields[static_cast<uint16>(UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE + i)] = 0;
  }

  // TODO: sum dodge/parry/crit/hit/haste/mastery ratings from equipment + auras.
  uint32 const dodgeRating = 0;
  uint32 const parryRating = 0;
  uint32 const critMeleeRating = 0;

  float dimDodge = 0.f;
  float nondimDodge = 0.f;
  StatFormulas::ComputeDodgeContributionsFromAgility(
      level, klass, agi, dimDodge, nondimDodge, statGameTables);
  dimDodge += StatFormulas::DodgeRatingToPercent(level, dodgeRating,
                                                 statGameTables);

  StatFormulas::AvoidanceClassParams const av =
      StatFormulas::AvoidanceParamsForClass(klass);
  float const dodgePct = StatFormulas::AvoidanceAfterDiminishingReturns(
      av.dodgeCap, av.diminishingK, nondimDodge, dimDodge);
  fields[PLAYER_DODGE_PERCENTAGE] = PackFloat(dodgePct);

  float parryPct = 0.0f;
  if (StatFormulas::ClassHasBaselineParry(klass)) {
    float const nondimParry = 5.0f;
    float const dimParry =
        StatFormulas::ParryRatingToPercent(level, parryRating, statGameTables);
    parryPct = StatFormulas::AvoidanceAfterDiminishingReturns(
        av.parryCap, av.diminishingK, nondimParry, dimParry);
  }
  fields[PLAYER_PARRY_PERCENTAGE] = PackFloat(parryPct);

  fields[PLAYER_BLOCK_PERCENTAGE] = PackFloat(0.0f);
  fields[PLAYER_SHIELD_BLOCK] = 0;
  fields[PLAYER_SHIELD_BLOCK_CRIT_PERCENTAGE] = PackFloat(0.0f);

  fields[PLAYER_EXPERTISE] = 0;
  fields[PLAYER_OFFHAND_EXPERTISE] = 0;

  float const meleeCritPct = StatFormulas::PhysicalCritPercent(
      level, klass, agi, critMeleeRating, statGameTables);
  fields[PLAYER_CRIT_PERCENTAGE] = PackFloat(meleeCritPct);
  fields[PLAYER_RANGED_CRIT_PERCENTAGE] = PackFloat(meleeCritPct);
  fields[PLAYER_OFFHAND_CRIT_PERCENTAGE] = PackFloat(meleeCritPct);

  fields[PLAYER_FIELD_UI_HIT_MODIFIER] = PackFloat(
      StatFormulas::MeleeHitPercentFromRating(level, 0, statGameTables));
  fields[PLAYER_MASTERY] = PackFloat(
      StatFormulas::MasteryPercentFromRating(level, 0, statGameTables));
}

} // namespace

void ApplyMovementHintsToPlayerCreateFields(std::map<uint16, uint32> &fields,
                                            MovementInfo const &move) {
  /// `UnitDefines` / client: swim-capable player (shows swim animation in liquid).
  uint32 uf = 0;
  if (auto it = fields.find(static_cast<uint16>(UNIT_FIELD_FLAGS));
      it != fields.end())
    uf = it->second;
  uf |= kUnitFlagCanSwim;
  fields[static_cast<uint16>(UNIT_FIELD_FLAGS)] = uf;

  uint8 const stand = 0;
  uint8 const petTalents = 0;
  uint8 const visFlags = 0;
  uint8 const animTier = MovementAnimTier(move);
  uint32 const bytes1 =
      uint32(stand) | (uint32(petTalents) << 8) | (uint32(visFlags) << 16) |
      (uint32(animTier) << 24);
  fields[static_cast<uint16>(UNIT_FIELD_BYTES_1)] = bytes1;
}

void BuildPlayerMovementHintsValuesUpdate(uint16 mapId, uint64 playerGuid,
                                          MovementInfo const &move,
                                          PlayerGmAppearanceForUpdates const &gmAppearance,
                                          WorldPacket &outPacket) {
  std::map<uint16, uint32> fields;
  MergeGmAppearanceIntoPlayerFields(fields, gmAppearance);
  ApplyMovementHintsToPlayerCreateFields(fields, move);
  UpdateData update(mapId);
  update.AddValuesUpdate(playerGuid, fields);
  update.Build(outPacket);
}

std::vector<uint32> BuildDefaultKnownSpells(uint8 classId) {
  switch (ToPlayerClass(classId)) {
  case PlayerClass::Warrior:
    return {2457, 71, 78, 100, 6673, 772, 3127, 34428};
  case PlayerClass::Paladin:
    return {465, 635, 20154, 20271, 35395, 19740, 498, 633, 82242};
  case PlayerClass::Hunter:
    return {75, 883, 13165, 1978, 2643, 56641, 781, 1130, 2973};
  case PlayerClass::Rogue:
    return {1784, 2098, 53, 1752, 921, 1766, 1776, 82245};
  case PlayerClass::Priest:
    return {585, 589, 2061, 17, 139, 2050, 8092};
  case PlayerClass::DeathKnight:
    return {48263, 45524, 49998, 47528, 48721, 45529, 48792};
  case PlayerClass::Shaman:
    return {331, 8042, 8017, 8050, 324, 51730, 8004, 52127};
  case PlayerClass::Mage:
    return {116, 133, 2136, 1459, 130, 1953, 118};
  case PlayerClass::Warlock:
    return {686, 172, 348, 1454, 5782, 980};
  case PlayerClass::Druid:
    return {8921, 5185, 774, 768, 1126, 339, 467};
  default:
    return {6673, 78, 2457, 3127};
  }
}

std::map<uint16, uint32> BuildItemCreateFields(uint64 itemObjectGuid,
                                               uint64 ownerGuid, uint32 itemEntry,
                                               uint32 stackCount) {
  std::map<uint16, uint32> fields;
  for (uint16_t i = 0; i < ITEM_END; ++i)
    fields[i] = 0;

  uint32 igLo = 0;
  uint32 igHi = 0;
  uint32 owLo = 0;
  uint32 owHi = 0;
  WriteGuidToTwoUint32(itemObjectGuid, igLo, igHi);
  WriteGuidToTwoUint32(ownerGuid, owLo, owHi);

  fields[OBJECT_FIELD_GUID] = igLo;
  fields[OBJECT_FIELD_GUID + 1] = igHi;
  fields[OBJECT_FIELD_DATA] = 0;
  fields[OBJECT_FIELD_DATA + 1] = 0;
  fields[OBJECT_FIELD_TYPE] = kTypeMaskItem;
  fields[OBJECT_FIELD_ENTRY] = itemEntry;
  fields[OBJECT_FIELD_SCALE_X] = 0x3F800000;
  fields[OBJECT_FIELD_PADDING] = 0;

  fields[ITEM_FIELD_OWNER] = owLo;
  fields[ITEM_FIELD_OWNER + 1] = owHi;
  fields[ITEM_FIELD_CONTAINED] = owLo;
  fields[ITEM_FIELD_CONTAINED + 1] = owHi;
  fields[ITEM_FIELD_STACK_COUNT] = stackCount;
  return fields;
}

std::map<uint16, uint32> BuildPlayerBag0InventoryValues(Character const &character) {
  std::map<uint16, uint32> fields;
  for (size_t slot = 0; slot < kEquipmentSlotCount; ++slot) {
    uint32 const entry = character.GetVisibleItemEntry(slot);
    uint32 const itemGuidLow = character.GetVisibleItemGuidLow(slot);
    uint16 const base = static_cast<uint16>(
        PLAYER_VISIBLE_ITEM_1_ENTRYID + static_cast<uint16>(slot * 2));
    fields[base] = entry;
    fields[static_cast<uint16>(base + 1)] = 0;

    uint64 const itemOg = MakeItemObjectGuid(itemGuidLow);
    uint32 ilo = 0;
    uint32 ihi = 0;
    WriteGuidToTwoUint32(itemOg, ilo, ihi);
    uint16 const invBase = static_cast<uint16>(
        PLAYER_FIELD_INV_SLOT_HEAD + static_cast<uint16>(slot * 2));
    fields[invBase] = ilo;
    fields[static_cast<uint16>(invBase + 1)] = ihi;
  }
  for (size_t packIndex = 0; packIndex < kPackSlotCount; ++packIndex) {
    uint32 const itemGuidLow = character.GetPackItemGuidLow(packIndex);
    uint64 const itemOg = MakeItemObjectGuid(itemGuidLow);
    uint32 ilo = 0;
    uint32 ihi = 0;
    WriteGuidToTwoUint32(itemOg, ilo, ihi);
    uint16 const packBase = static_cast<uint16>(
        PLAYER_FIELD_PACK_SLOT_1 + static_cast<uint16>(packIndex * 2));
    fields[packBase] = ilo;
    fields[static_cast<uint16>(packBase + 1)] = ihi;
  }
  return fields;
}

std::map<uint16, uint32> BuildPlayerUpdateFields(
    uint64 guid, Character const &character,
    GtPlayerStatGameTables const *statGameTables,
    uint32_t nextLevelXpFromWorld,
    std::optional<std::pair<uint32, uint32>> const &healthOverride,
    std::optional<std::pair<uint32, uint32>> const &power1Override,
    std::vector<StarterSkillGrant> const &starterSkills) {
  std::map<uint16, uint32> fields;
  fields[OBJECT_FIELD_GUID] = (uint32)(guid & 0xFFFFFFFF);
  fields[OBJECT_FIELD_GUID + 1] = (uint32)(guid >> 32);
  fields[OBJECT_FIELD_TYPE] =
      (1 << TYPEID_OBJECT) | (1 << TYPEID_UNIT) | (1 << TYPEID_PLAYER);
  fields[OBJECT_FIELD_SCALE_X] = 0x3F800000;

  uint8 bytes0[4] = {character.GetRace(), ToClassId(character.GetClass()),
                     character.GetGender(), character.GetPowerType()};
  std::memcpy(&fields[UNIT_FIELD_BYTES_0], bytes0, 4);

  if (healthOverride) {
    fields[UNIT_FIELD_HEALTH] = healthOverride->first;
    fields[UNIT_FIELD_MAXHEALTH] = healthOverride->second;
  } else {
    fields[UNIT_FIELD_HEALTH] = character.GetHealth();
    fields[UNIT_FIELD_MAXHEALTH] = character.GetMaxHealth();
  }
  if (power1Override) {
    fields[UNIT_FIELD_POWER1] = power1Override->first;
    fields[UNIT_FIELD_MAXPOWER1] = power1Override->second;
  } else {
    fields[UNIT_FIELD_POWER1] = character.GetPower1();
    fields[UNIT_FIELD_MAXPOWER1] = character.GetMaxPower1();
  }
  fields[UNIT_FIELD_BASE_HEALTH] = fields[UNIT_FIELD_MAXHEALTH];
  fields[UNIT_FIELD_BASE_MANA] = fields[UNIT_FIELD_MAXPOWER1];
  fields[UNIT_FIELD_LEVEL] = character.GetLevel();
  for (uint8_t i = 0; i < 5; ++i) {
    fields[static_cast<uint16>(UNIT_FIELD_STAT0 + i)] =
        character.GetPrimaryStat(i);
    // Bonuses from gear/auras (zero until item/aura systems populate them).
    fields[static_cast<uint16>(UNIT_FIELD_POSSTAT0 + i)] = 0;
    fields[static_cast<uint16>(UNIT_FIELD_NEGSTAT0 + i)] = 0;
  }
  fields[UNIT_FIELD_FACTIONTEMPLATE] = character.GetFactionTemplate();
  fields[UNIT_FIELD_DISPLAYID] = character.GetDisplayId();
  fields[UNIT_FIELD_NATIVEDISPLAYID] = character.GetDisplayId();
  fields[UNIT_FIELD_BYTES_2] = 0;

  // Action bar experience vs reputation: watched faction index -1 (as uint32 0xFFFFFFFF)
  // means track XP. If this field is omitted from the create update, the client
  // defaults to slot 0 and shows that faction's name/rep bar instead of experience.
  fields[PLAYER_FIELD_WATCHED_FACTION_INDEX] = static_cast<uint32>(-1);
  SetPlayerFieldByte(fields, PLAYER_FIELD_BYTES,
                     PLAYER_FIELD_BYTES_OFFSET_ACTION_BAR_TOGGLES,
                     character.GetActionBarToggles());
  {
    uint8 const lv = character.GetLevel();
    constexpr uint8 kMaxLevelCata = 85;
    if (lv >= kMaxLevelCata) {
      fields[PLAYER_XP] = 0;
      fields[PLAYER_NEXT_LEVEL_XP] = 0;
      ApplyRestExperienceWireFields(fields, 0.f, character.GetFacialHair());
    } else {
      uint32_t next = nextLevelXpFromWorld;
      if (next == 0)
        next = 400u;
      uint32_t xp = character.GetXp();
      if (xp > next)
        xp = next;
      fields[PLAYER_XP] = xp;
      fields[PLAYER_NEXT_LEVEL_XP] = next;
      ApplyRestExperienceWireFields(fields, character.GetRestBonus(),
                                    character.GetFacialHair());
    }
  }

  AddBaselineMeleeFields(fields, character);
  AddBaselineSpellFields(fields, character, statGameTables);
  AddBaselineDefenseAndResistanceFields(fields, character, statGameTables);

  fields[PLAYER_PROFESSION_SKILL_LINE_1] = 0;
  fields[static_cast<uint16>(PLAYER_PROFESSION_SKILL_LINE_1 + 1)] = 0;

  if (!starterSkills.empty())
    AddPlayerSkillFields(fields, starterSkills);
  else {
    std::vector<StarterSkillGrant> langOnly;
    std::vector<uint32> langIds;
    AppendRacialLanguageSkills(character.GetRace(), langIds);
    for (uint32 sid : langIds) {
      StarterSkillGrant g;
      g.skillId = sid;
      g.rank = 300;
      g.maxRank = 300;
      langOnly.push_back(g);
    }
    AddPlayerSkillFields(fields, langOnly);
  }

  {
    uint64 const coin = character.GetMoney();
    fields[PLAYER_FIELD_COINAGE] = static_cast<uint32>(coin & 0xFFFFFFFFu);
    fields[static_cast<uint16>(PLAYER_FIELD_COINAGE + 1)] =
        static_cast<uint32>((coin >> 32) & 0xFFFFFFFFu);
  }

  for (int i = 0; i < 26; ++i) {
    fields[static_cast<uint16>(PLAYER_FIELD_COMBAT_RATING_1 +
                               static_cast<uint16>(i))] = 0;
  }

  for (size_t slot = 0; slot < kEquipmentSlotCount; ++slot) {
    uint32 const entry = character.GetVisibleItemEntry(slot);
    uint32 const itemGuidLow = character.GetVisibleItemGuidLow(slot);
    uint16 const base = static_cast<uint16>(
        PLAYER_VISIBLE_ITEM_1_ENTRYID + static_cast<uint16>(slot * 2));
    fields[base] = entry;
    fields[static_cast<uint16>(base + 1)] = 0;

    uint64 const itemOg = MakeItemObjectGuid(itemGuidLow);
    uint32 ilo = 0;
    uint32 ihi = 0;
    WriteGuidToTwoUint32(itemOg, ilo, ihi);
    uint16 const invBase = static_cast<uint16>(
        PLAYER_FIELD_INV_SLOT_HEAD + static_cast<uint16>(slot * 2));
    fields[invBase] = ilo;
    fields[static_cast<uint16>(invBase + 1)] = ihi;
  }
  for (size_t packIndex = 0; packIndex < kPackSlotCount; ++packIndex) {
    uint32 const itemGuidLow = character.GetPackItemGuidLow(packIndex);
    uint64 const itemOg = MakeItemObjectGuid(itemGuidLow);
    uint32 ilo = 0;
    uint32 ihi = 0;
    WriteGuidToTwoUint32(itemOg, ilo, ihi);
    uint16 const packBase = static_cast<uint16>(
        PLAYER_FIELD_PACK_SLOT_1 + static_cast<uint16>(packIndex * 2));
    fields[packBase] = ilo;
    fields[static_cast<uint16>(packBase + 1)] = ihi;
  }

  // Reference `Player::Create`: client power regen requires this flag on login.
  fields[UNIT_FIELD_FLAGS_2] = UNIT_FLAG2_REGENERATE_POWER;

  return fields;
}

void BuildUnitHealthValuesUpdate(uint16 mapId, uint64 unitGuid, uint32 health,
                                 uint32 maxHealth, WorldPacket &outPacket) {
  std::map<uint16, uint32> fields;
  fields[UNIT_FIELD_HEALTH] = health;
  fields[UNIT_FIELD_MAXHEALTH] = maxHealth;
  UpdateData update(mapId);
  update.AddValuesUpdate(unitGuid, fields);
  update.Build(outPacket);
}

void BuildPlayerHealthValuesUpdate(uint16 mapId, uint64 playerGuid, uint32 health,
                                   uint32 maxHealth, WorldPacket &outPacket) {
  BuildUnitHealthValuesUpdate(mapId, playerGuid, health, maxHealth, outPacket);
}

void BuildPlayerPower1ValuesUpdate(uint16 mapId, uint64 playerGuid, uint32 power1,
                                   uint32 maxPower1, WorldPacket &outPacket) {
  std::map<uint16, uint32> fields;
  fields[UNIT_FIELD_POWER1] = power1;
  fields[UNIT_FIELD_MAXPOWER1] = maxPower1;
  UpdateData update(mapId);
  update.AddValuesUpdate(playerGuid, fields);
  update.Build(outPacket);
}

void BuildPlayerRestStateValuesUpdate(uint16 mapId, uint64 playerGuid, float restBonus,
                                      uint8_t facialHair, WorldPacket &outPacket) {
  std::map<uint16, uint32> fields;
  ApplyRestExperienceWireFields(fields, restBonus, facialHair);
  UpdateData update(mapId);
  update.AddValuesUpdate(playerGuid, fields);
  update.Build(outPacket);
}

void BuildPlayerActionBarTogglesValuesUpdate(uint16 mapId, uint64 playerGuid,
                                             uint8 actionBarToggles,
                                             WorldPacket &outPacket) {
  std::map<uint16, uint32> fields;
  SetPlayerFieldByte(fields, PLAYER_FIELD_BYTES,
                     PLAYER_FIELD_BYTES_OFFSET_ACTION_BAR_TOGGLES, actionBarToggles);
  UpdateData update(mapId);
  update.AddValuesUpdate(playerGuid, fields);
  update.Build(outPacket);
}

void MergeQuestLogIntoPlayerFields(std::map<uint16, uint32> &fields,
                                   PlayerQuestProgressStore const &progress) {
  for (uint8_t slot = 0; slot < kMaxQuestLogSlots; ++slot) {
    uint32_t const questId = progress.GetQuestLogSlotQuestId(slot);
    if (questId == 0)
      continue;
    uint32_t stateFlags = 0;
    if (progress.GetQuestStatus(questId) == QuestStatus::Complete)
      stateFlags = kPlayerQuestLogStateComplete;
    WriteQuestLogSlotFields(fields, slot, questId, stateFlags, 0u);
  }
}

void BuildPlayerQuestLogSlotValuesUpdate(uint16 mapId, uint64 playerGuid, uint8 slot,
                                         uint32 questId, uint32 questStateFlags,
                                         uint32 timerMs, WorldPacket &outPacket) {
  if (slot >= kMaxQuestLogSlots)
    return;
  std::map<uint16, uint32> fields;
  WriteQuestLogSlotFields(fields, slot, questId, questStateFlags, timerMs);
  UpdateData update(mapId);
  update.AddValuesUpdate(playerGuid, fields);
  update.Build(outPacket);
}

void BuildPlayerAuraStatValuesUpdate(uint16 mapId, uint64 playerGuid,
                                     PlayerAuraStatBonus const &bonus,
                                     WorldPacket &outPacket,
                                     UnitCombatStats const *baseline,
                                     float baselineDodgePct) {
  std::map<uint16, uint32> fields;
  for (uint8_t i = 0; i < 5; ++i) {
    fields[static_cast<uint16>(UNIT_FIELD_POSSTAT0 + i)] =
        static_cast<uint32>(std::max<int32>(0, bonus.posStat[i]));
    fields[static_cast<uint16>(UNIT_FIELD_NEGSTAT0 + i)] =
        static_cast<uint32>(std::max<int32>(0, bonus.negStat[i]));
  }
  for (size_t r = 0; r < bonus.combatRating.size(); ++r) {
    if (bonus.combatRating[r] != 0)
      fields[static_cast<uint16>(PLAYER_FIELD_COMBAT_RATING_1 +
                                 static_cast<uint16>(r))] =
          static_cast<uint32>(std::max<int32>(0, bonus.combatRating[r]));
  }
  fields[UNIT_FIELD_ATTACK_POWER_MOD_POS] =
      static_cast<uint32>(std::max<int32>(0, bonus.attackPowerModPos));
  fields[UNIT_FIELD_ATTACK_POWER_MOD_NEG] =
      static_cast<uint32>(std::max<int32>(0, bonus.attackPowerModNeg));
  fields[UNIT_FIELD_ATTACK_POWER_MULTIPLIER] = PackFloat(bonus.attackPowerMultiplier);
  if (bonus.dodgePctBonus != 0.f || baselineDodgePct != 0.f)
    fields[PLAYER_DODGE_PERCENTAGE] = PackFloat(baselineDodgePct + bonus.dodgePctBonus);
  for (uint8_t school = 0; school < 7; ++school) {
    int32 const basePos =
        baseline ? baseline->spellDamageDonePos[school] : 0;
    int32 const dmgPos = basePos + bonus.damageDonePos[school] - bonus.damageDoneNeg[school];
    if (dmgPos != 0 || bonus.damageDonePos[school] != 0 || bonus.damageDoneNeg[school] != 0)
      fields[static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + school)] =
          static_cast<uint32>(std::max<int32>(0, dmgPos));
    if (bonus.damageDonePctMultiplier[school] > 0.f)
      fields[static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_PCT + school)] =
          PackFloat(bonus.damageDonePctMultiplier[school]);
    int32 const resPos = bonus.resistanceBuffPos[school];
    int32 const resNeg = bonus.resistanceBuffNeg[school];
    if (resPos != 0)
      fields[static_cast<uint16>(UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE + school)] =
          static_cast<uint32>(std::max<int32>(0, resPos));
    if (resNeg != 0)
      fields[static_cast<uint16>(UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE + school)] =
          static_cast<uint32>(std::max<int32>(0, resNeg));
  }
  if (bonus.meleeHasteMultiplier != 1.f) {
    fields[PLAYER_FIELD_MOD_HASTE] = PackFloat(bonus.meleeHasteMultiplier);
    fields[PLAYER_FIELD_MOD_PET_HASTE] = PackFloat(bonus.meleeHasteMultiplier);
  }
  if (bonus.rangedHasteMultiplier != 1.f)
    fields[PLAYER_FIELD_MOD_RANGED_HASTE] = PackFloat(bonus.rangedHasteMultiplier);
  if (bonus.castHasteMultiplier != 1.f) {
    fields[UNIT_MOD_CAST_SPEED] = PackFloat(bonus.castHasteMultiplier);
    fields[UNIT_MOD_CAST_HASTE] = PackFloat(bonus.castHasteMultiplier);
  }
  UpdateData update(mapId);
  update.AddValuesUpdate(playerGuid, fields);
  update.Build(outPacket);
}

void BuildUnitFactionTemplateValuesUpdate(uint16 mapId, uint64 unitGuid,
                                          uint32 factionTemplate,
                                          WorldPacket &outPacket) {
  std::map<uint16, uint32> fields;
  fields[UNIT_FIELD_FACTIONTEMPLATE] = factionTemplate;
  UpdateData update(mapId);
  update.AddValuesUpdate(unitGuid, fields);
  update.Build(outPacket);
}

void BuildUnitNpcEmoteStateValuesUpdate(uint16 mapId, uint64 unitGuid,
                                        uint32 emoteState,
                                        WorldPacket &outPacket) {
  std::map<uint16, uint32> fields;
  fields[UNIT_NPC_EMOTESTATE] = emoteState;
  UpdateData update(mapId);
  update.AddValuesUpdate(unitGuid, fields);
  update.Build(outPacket);
}

void BuildUnitTargetValuesUpdate(uint16 mapId, uint64 unitGuid, uint64 targetGuid,
                                 WorldPacket &outPacket) {
  std::map<uint16, uint32> fields;
  uint32 lo = 0;
  uint32 hi = 0;
  WriteGuidToTwoUint32(targetGuid, lo, hi);
  fields[UNIT_FIELD_TARGET] = lo;
  fields[static_cast<uint16>(UNIT_FIELD_TARGET + 1)] = hi;
  UpdateData update(mapId);
  update.AddValuesUpdate(unitGuid, fields);
  update.Build(outPacket);
}

void BuildUnitFlagsValuesUpdate(uint16 mapId, uint64 unitGuid, uint32 unitFieldFlags,
                                WorldPacket &outPacket) {
  std::map<uint16, uint32> fields;
  fields[UNIT_FIELD_FLAGS] = unitFieldFlags;
  UpdateData update(mapId);
  update.AddValuesUpdate(unitGuid, fields);
  update.Build(outPacket);
}

void BuildUnitDynamicFlagsValuesUpdate(uint16 mapId, uint64 unitGuid,
                                       uint32 dynamicFlags, WorldPacket &outPacket) {
  std::map<uint16, uint32> fields;
  fields[UNIT_DYNAMIC_FLAGS] = dynamicFlags;
  UpdateData update(mapId);
  update.AddValuesUpdate(unitGuid, fields);
  update.Build(outPacket);
}

void AppendPlayerGuidLookupData(WorldPacket &dst, Character const &ch,
                                std::string const &realmName) {
  dst.WriteString(ch.GetName());
  dst.WriteString(realmName);
  dst.Append<uint8>(ch.GetRace());
  dst.Append<uint8>(ch.GetGender());
  dst.Append<uint8>(ToClassId(ch.GetClass()));
  dst.Append<uint8>(0); // DeclinedNames.has_value() == false
}

uint64 ReadClientTargetGuid(WorldPacket &packet) {
  const size_t rem = packet.Size() - packet.GetReadPos();
  if (rem >= sizeof(uint64)) {
    // CMSG_GOSSIP_HELLO / SELECT: Cataclysm 4.3.4 sends a full ObjectGuid (8 bytes).
    return packet.Read<uint64>();
  }
  if (rem > 0) {
    return packet.ReadPackedGuid();
  }
  return 0;
}

uint64 ReadClientQuestGiverGuid(WorldPacket &packet) {
  // CMSG_QUESTGIVER_STATUS_QUERY uses a full 8-byte ObjectGuid on 4.3.4 (see world log:
  // opcode 0x4407, payload 8). CMSG_QUESTGIVER_HELLO may use packed form — same rule as
  // gossip hello: uint64 when 8 bytes remain, else packed.
  return ReadClientTargetGuid(packet);
}

void ReadQuestGiverClientTail(WorldPacket &packet) {
  if (packet.GetReadPos() + sizeof(uint32_t) <= packet.Size())
    (void)packet.Read<uint32_t>();
}

void SendPlayerCreateToNotifier(
    std::shared_ptr<IMapNotifier> target, uint32 mapId, uint64 objectGuid,
    Character const &character, MovementInfo const &move,
    PlayerGmAppearanceForUpdates const &gmAppearance,
    GtPlayerStatGameTables const *statGameTables,
    uint32_t nextLevelXpFromWorld,
    std::optional<std::pair<uint32, uint32>> const &healthOverride,
    std::optional<std::pair<uint32, uint32>> const &power1Override) {
  if (!target)
    return;
  auto fields = BuildPlayerUpdateFields(objectGuid, character, statGameTables,
                                        nextLevelXpFromWorld, healthOverride,
                                        power1Override);
  MergeGmAppearanceIntoPlayerFields(fields, gmAppearance);
  ApplyMovementHintsToPlayerCreateFields(fields, move);
  UpdateData update(mapId);
  update.AddCreateObject(objectGuid, TYPEID_PLAYER, move, fields);
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  target->SendPacket(pkt);
}

std::map<uint16, uint32> BuildMinimalNpcUnitCreateFields(
    uint64 objectGuid, uint32 creatureEntry, uint32 displayId, uint32 health,
    uint32 maxHealth, uint8 level, uint32 npcFlags, uint32 factionTemplate,
    uint32 unitFieldFlags, uint32 unitFieldFlags2, uint32 unitDynamicFlags,
    UnitCombatStats const *combatStats) {
  auto const packF = [](float v) {
    uint32 b = 0;
    std::memcpy(&b, &v, sizeof(b));
    return b;
  };

  std::map<uint16, uint32> fields;
  fields[OBJECT_FIELD_GUID] = static_cast<uint32>(objectGuid & 0xFFFFFFFFu);
  fields[OBJECT_FIELD_GUID + 1] = static_cast<uint32>(objectGuid >> 32);
  fields[OBJECT_FIELD_TYPE] =
      (1u << TYPEID_OBJECT) | (1u << TYPEID_UNIT);
  fields[OBJECT_FIELD_ENTRY] = creatureEntry;
  fields[OBJECT_FIELD_SCALE_X] = packF(1.0f);
  fields[OBJECT_FIELD_PADDING] = 0;

  uint8 bytes0[4] = {1, 8, 0, 0}; // Human Mage, male, mana (creature visuals only)
  std::memcpy(&fields[UNIT_FIELD_BYTES_0], bytes0, 4);

  fields[UNIT_FIELD_HEALTH] = health;
  fields[UNIT_FIELD_MAXHEALTH] = maxHealth;
  fields[UNIT_FIELD_POWER1] = 100;
  fields[UNIT_FIELD_POWER2] = 0;
  fields[UNIT_FIELD_POWER3] = 0;
  fields[UNIT_FIELD_POWER4] = 0;
  fields[UNIT_FIELD_POWER5] = 0;
  fields[UNIT_FIELD_MAXPOWER1] = 100;
  fields[UNIT_FIELD_MAXPOWER2] = 0;
  fields[UNIT_FIELD_MAXPOWER3] = 0;
  fields[UNIT_FIELD_MAXPOWER4] = 0;
  fields[UNIT_FIELD_MAXPOWER5] = 0;

  fields[UNIT_FIELD_LEVEL] = level;
  fields[UNIT_FIELD_FACTIONTEMPLATE] = factionTemplate;
  fields[UNIT_VIRTUAL_ITEM_SLOT_ID] = 0;
  fields[static_cast<uint16>(UNIT_VIRTUAL_ITEM_SLOT_ID + 1)] = 0;
  fields[static_cast<uint16>(UNIT_VIRTUAL_ITEM_SLOT_ID + 2)] = 0;

  fields[UNIT_FIELD_FLAGS] = unitFieldFlags;
  fields[UNIT_FIELD_FLAGS_2] = unitFieldFlags2;
  fields[UNIT_FIELD_AURASTATE] = 0;
  fields[UNIT_FIELD_BASEATTACKTIME] = 2000;
  fields[static_cast<uint16>(UNIT_FIELD_BASEATTACKTIME + 1)] = 2000;
  fields[UNIT_FIELD_RANGEDATTACKTIME] = 2000;
  fields[UNIT_FIELD_BOUNDINGRADIUS] = packF(0.306f);
  fields[UNIT_FIELD_COMBATREACH] = packF(1.5f);
  fields[UNIT_FIELD_DISPLAYID] = displayId;
  fields[UNIT_FIELD_NATIVEDISPLAYID] = displayId;
  fields[UNIT_FIELD_MOUNTDISPLAYID] = 0;
  fields[UNIT_FIELD_MINOFFHANDDAMAGE] = packF(0.0f);
  fields[UNIT_FIELD_MAXOFFHANDDAMAGE] = packF(0.0f);
  fields[UNIT_FIELD_BYTES_1] = 0;
  fields[UNIT_FIELD_PETNUMBER] = 0;
  fields[UNIT_FIELD_PET_NAME_TIMESTAMP] = 0;
  fields[UNIT_FIELD_PETEXPERIENCE] = 0;
  fields[UNIT_FIELD_PETNEXTLEVELEXP] = 0;
  fields[UNIT_DYNAMIC_FLAGS] = unitDynamicFlags;
  fields[UNIT_MOD_CAST_SPEED] = packF(1.0f);
  fields[UNIT_MOD_CAST_HASTE] = packF(0.0f);
  fields[UNIT_CREATED_BY_SPELL] = 0;
  fields[UNIT_NPC_FLAGS] = npcFlags;
  fields[UNIT_NPC_EMOTESTATE] = 0;

  for (uint16_t i = 0; i < 5; ++i) {
    fields[static_cast<uint16>(UNIT_FIELD_STAT0 + i)] = 10;
    fields[static_cast<uint16>(UNIT_FIELD_POSSTAT0 + i)] = 0;
    fields[static_cast<uint16>(UNIT_FIELD_NEGSTAT0 + i)] = 0;
  }

  fields[UNIT_FIELD_BASE_MANA] = 100;
  fields[UNIT_FIELD_BASE_HEALTH] = maxHealth;
  fields[UNIT_FIELD_BYTES_2] = 0;

  int32 const ap = combatStats ? EffectiveAttackPower(*combatStats) : 0;
  fields[UNIT_FIELD_ATTACK_POWER] = static_cast<uint32>(std::max(0, ap));
  fields[UNIT_FIELD_ATTACK_POWER_MOD_POS] = 0;
  fields[UNIT_FIELD_ATTACK_POWER_MOD_NEG] = 0;
  fields[UNIT_FIELD_ATTACK_POWER_MULTIPLIER] = packF(0.0f);
  if (combatStats && ap > 0) {
    constexpr uint32 kAttackTimeMs = 2000u;
    float const speedSec = static_cast<float>(kAttackTimeMs) / 1000.0f;
    float const dpsFromAp = static_cast<float>(ap) / 14.0f;
    fields[UNIT_FIELD_MINDAMAGE] = packF(1.0f + dpsFromAp * speedSec * 0.85f);
    fields[UNIT_FIELD_MAXDAMAGE] = packF(2.0f + dpsFromAp * speedSec * 1.15f);
  } else {
    fields[UNIT_FIELD_MINDAMAGE] = packF(1.0f);
    fields[UNIT_FIELD_MAXDAMAGE] = packF(2.0f);
  }
  fields[UNIT_FIELD_HOVERHEIGHT] = packF(1.0f);

  return fields;
}

void BuildCreatureQueryResponse(
    WorldPacket &out, uint32 creatureEntry,
    std::optional<std::pair<std::string, std::string>> const &nameTitle,
    std::array<uint32, 4> const &creatureDisplayIds) {
  // Sizes match `CreatureStats` / `QueryPackets.cpp`.
  static constexpr uint32 kKillCredits = 2;
  static constexpr uint32 kCreatureModels = 4;
  static constexpr uint32 kCreatureQuestItems = 6;

  bool const allow = nameTitle.has_value();
  out.Clear();
  out.SetOpcode(SMSG_CREATURE_QUERY_RESPONSE);
  out.Append<uint32>(creatureEntry | (allow ? 0u : 0x80000000u));
  if (!allow)
    return;

  std::string const &creatureName = nameTitle->first;
  std::string const &subname = nameTitle->second;

  for (int i = 0; i < 4; ++i)
    out.WriteString(i == 0 ? creatureName : std::string());
  for (int i = 0; i < 4; ++i)
    out.WriteString(std::string());
  out.WriteString(subname);
  out.WriteString(std::string());

  out.Append<uint32>(0);
  out.Append<uint32>(0);

  out.Append<uint32>(0);
  out.Append<uint32>(0);
  out.Append<uint32>(0);

  for (uint32 i = 0; i < kKillCredits; ++i)
    out.Append<uint32>(0);

  for (uint32 i = 0; i < kCreatureModels; ++i)
    out.Append<uint32>(creatureDisplayIds[i]);

  out.Append<float>(1.0f);
  out.Append<float>(1.0f);
  out.Append<uint8>(0);

  for (uint32 i = 0; i < kCreatureQuestItems; ++i)
    out.Append<uint32>(0);

  out.Append<uint32>(0);
  out.Append<uint32>(0);
}

} // namespace WorldSessionObjectUpdate
} // namespace Firelands
