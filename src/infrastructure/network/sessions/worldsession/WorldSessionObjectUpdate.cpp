#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/network/UpdateData.h>
#include <shared/network/UpdateFields.h>
#include <shared/game/ChatLanguages.h>
#include <shared/game/EquipmentCache.h>
#include <shared/dbc/GtPlayerStatGameTables.h>
#include <shared/game/StatFormulas.h>
#include <shared/game/InventorySlots.h>
#include <shared/game/WowGuid.h>

#include <algorithm>
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

void AddLanguageSkillFields(std::map<uint16, uint32> &fields, uint8 race) {
  std::vector<uint32> skills;
  AppendRacialLanguageSkills(race, skills);
  constexpr uint16 kRank = 300;
  for (size_t i = 0; i < skills.size() && i < 64; ++i) {
    uint8 const slot = static_cast<uint8>(i);
    uint16 const skill = static_cast<uint16>(skills[i]);
    SetPackedShort(fields, PLAYER_SKILL_LINEID_0, slot, skill);
    SetPackedShort(fields, PLAYER_SKILL_STEP_0, slot, 0);
    SetPackedShort(fields, PLAYER_SKILL_RANK_0, slot, kRank);
    SetPackedShort(fields, PLAYER_SKILL_MAX_RANK_0, slot, kRank);
    SetPackedShort(fields, PLAYER_SKILL_MODIFIER_0, slot, 0);
    SetPackedShort(fields, PLAYER_SKILL_TALENT_0, slot, 0);
  }
}

uint32 PackFloat(float value) {
  uint32 bits = 0;
  static_assert(sizeof(bits) == sizeof(value), "float packing");
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

bool UsesAgilityForMeleeAttackPower(uint8 classId) {
  // Simplified baseline until full stat system / shapeshift auras are wired.
  return classId == 3 || classId == 4 || classId == 11;
}

int32 ComputeBaseMeleeAttackPower(Character const &character) {
  uint32 const str = character.GetPrimaryStat(0);
  uint32 const agi = character.GetPrimaryStat(1);
  uint32 const level = character.GetLevel();
  int32 ap = static_cast<int32>(level) * 3;
  if (UsesAgilityForMeleeAttackPower(character.GetClass()))
    ap += static_cast<int32>(agi) * 2;
  else
    ap += static_cast<int32>(str) * 2;
  return ap < 0 ? 0 : ap;
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
}

/// Classes that should show non-trivial spell/healing numbers in the Spell panel
/// before gear, talents, and auras are implemented.
bool UsesBaselineSpellPowerFromIntellect(uint8 classId) {
  switch (classId) {
  case 2:  // Paladin
  case 3:  // Hunter (minimal magical scaling; still avoids an empty pane)
  case 5:  // Priest
  case 6:  // Death Knight
  case 7:  // Shaman
  case 8:  // Mage
  case 9:  // Warlock
  case 11: // Druid
    return true;
  default:
    return false;
  }
}

bool UsesBaselineHealingPower(uint8 classId) {
  return classId == 2 || classId == 5 || classId == 7 || classId == 11;
}

void AddBaselineSpellFields(std::map<uint16, uint32> &fields,
                            Character const &character,
                            GtPlayerStatGameTables const *statGameTables) {
  uint8 const klass = character.GetClass();
  uint8 const level = character.GetLevel();
  uint32 const inte = character.GetPrimaryStat(3);
  uint32 const spi = character.GetPrimaryStat(4);

  // Cast speed / haste multipliers on the unit (1.0 = baseline until aura system).
  fields[UNIT_MOD_CAST_SPEED] = PackFloat(1.0f);
  fields[UNIT_MOD_CAST_HASTE] = PackFloat(0.0f);

  // Player spell haste modifier (rating → multiplier comes later).
  fields[PLAYER_FIELD_MOD_HASTE] = PackFloat(0.0f);
  fields[PLAYER_FIELD_MOD_RANGED_HASTE] = PackFloat(0.0f);
  fields[PLAYER_FIELD_MOD_PET_HASTE] = PackFloat(0.0f);
  fields[PLAYER_FIELD_MOD_HASTE_REGEN] = PackFloat(0.0f);

  fields[PLAYER_FIELD_UI_SPELL_HIT_MODIFIER] = PackFloat(
      StatFormulas::SpellHitPercentFromRating(level, 0, statGameTables));

  fields[PLAYER_FIELD_MOD_SPELL_POWER_PCT] = PackFloat(1.0f);
  fields[PLAYER_FIELD_OVERRIDE_SPELL_POWER_BY_AP_PCT] = PackFloat(0.0f);

  for (uint32_t i = 0; i < 7; ++i) {
    fields[static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i)] = 0;
    fields[static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + i)] = 0;
    fields[static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_PCT + i)] = 0;
  }

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

uint32 ComputeBaselineArmor(uint8 classId, uint8 level, uint32 agi, uint32 str,
                            uint32 sta) {
  uint32 const lv = static_cast<uint32>(level);
  uint32 a = lv * 12u;
  switch (classId) {
  case 1:  // Warrior
  case 2:  // Paladin
  case 6:  // Death Knight
    a += sta * 2u + str + agi / 2u;
    break;
  case 3:  // Hunter
  case 7:  // Shaman
    a += agi * 3u + sta + lv;
    break;
  case 4:  // Rogue
  case 11: // Druid
    a += agi * 4u + lv * 2u;
    break;
  default: // Cloth casters
    a += lv * 6u + sta;
    break;
  }
  return std::max(10u, a);
}

void AddBaselineDefenseAndResistanceFields(
    std::map<uint16, uint32> &fields, Character const &character,
    GtPlayerStatGameTables const *statGameTables) {
  uint8 const klass = character.GetClass();
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

std::vector<uint32> BuildDefaultKnownSpells(uint8 classId) {
  switch (classId) {
  case 1: // Warrior
    return {2457, 71, 78, 100, 6673, 772, 3127, 34428};
  case 2: // Paladin
    return {465, 635, 20154, 20271, 19740, 498, 633, 82242};
  case 3: // Hunter
    return {75, 13165, 1978, 3044, 56641, 781, 1130, 2973};
  case 4: // Rogue
    return {1784, 2098, 53, 1752, 921, 1766, 1776, 82245};
  case 5: // Priest
    return {585, 589, 2061, 17, 139, 2050, 8092};
  case 6: // Death Knight
    return {48263, 45524, 49998, 47528, 48721, 45529, 48792};
  case 7: // Shaman
    return {331, 8042, 8017, 8050, 324, 51730, 5185, 52127};
  case 8: // Mage
    return {116, 133, 2136, 1459, 130, 1953, 118};
  case 9: // Warlock
    return {172, 348, 687, 1454, 5782, 980, 603};
  case 11: // Druid
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
    std::optional<std::pair<uint32, uint32>> const &healthOverride) {
  std::map<uint16, uint32> fields;
  fields[OBJECT_FIELD_GUID] = (uint32)(guid & 0xFFFFFFFF);
  fields[OBJECT_FIELD_GUID + 1] = (uint32)(guid >> 32);
  fields[OBJECT_FIELD_TYPE] =
      (1 << TYPEID_OBJECT) | (1 << TYPEID_UNIT) | (1 << TYPEID_PLAYER);
  fields[OBJECT_FIELD_SCALE_X] = 0x3F800000;

  uint8 bytes0[4] = {character.GetRace(), character.GetClass(),
                     character.GetGender(), character.GetPowerType()};
  std::memcpy(&fields[UNIT_FIELD_BYTES_0], bytes0, 4);

  if (healthOverride) {
    fields[UNIT_FIELD_HEALTH] = healthOverride->first;
    fields[UNIT_FIELD_MAXHEALTH] = healthOverride->second;
  } else {
    fields[UNIT_FIELD_HEALTH] = character.GetHealth();
    fields[UNIT_FIELD_MAXHEALTH] = character.GetMaxHealth();
  }
  fields[UNIT_FIELD_POWER1] = character.GetPower1();
  fields[UNIT_FIELD_MAXPOWER1] = character.GetMaxPower1();
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
  {
    uint8 const lv = character.GetLevel();
    constexpr uint8 kMaxLevelCata = 85;
    if (lv >= kMaxLevelCata) {
      fields[PLAYER_XP] = 0;
      fields[PLAYER_NEXT_LEVEL_XP] = 0;
    } else {
      uint32_t next = nextLevelXpFromWorld;
      if (next == 0)
        next = 400u;
      uint32_t xp = character.GetXp();
      if (xp > next)
        xp = next;
      fields[PLAYER_XP] = xp;
      fields[PLAYER_NEXT_LEVEL_XP] = next;
    }
  }

  AddBaselineMeleeFields(fields, character);
  AddBaselineSpellFields(fields, character, statGameTables);
  AddBaselineDefenseAndResistanceFields(fields, character, statGameTables);

  AddLanguageSkillFields(fields, character.GetRace());

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

  return fields;
}

void BuildPlayerHealthValuesUpdate(uint16 mapId, uint64 playerGuid, uint32 health,
                                   uint32 maxHealth, WorldPacket &outPacket) {
  std::map<uint16, uint32> fields;
  fields[UNIT_FIELD_HEALTH] = health;
  fields[UNIT_FIELD_MAXHEALTH] = maxHealth;
  UpdateData update(mapId);
  update.AddValuesUpdate(playerGuid, fields);
  update.Build(outPacket);
}

void AppendPlayerGuidLookupData(WorldPacket &dst, Character const &ch,
                                std::string const &realmName) {
  dst.WriteString(ch.GetName());
  dst.WriteString(realmName);
  dst.Append<uint8>(ch.GetRace());
  dst.Append<uint8>(ch.GetGender());
  dst.Append<uint8>(ch.GetClass());
  dst.Append<uint8>(0); // DeclinedNames.has_value() == false
}

uint64 ReadClientTargetGuid(WorldPacket &packet) {
  const size_t rem = packet.Size() - packet.GetReadPos();
  if (rem >= sizeof(uint64)) {
    return packet.Read<uint64>();
  }
  if (rem > 0) {
    return packet.ReadPackedGuid();
  }
  return 0;
}

void SendPlayerCreateToNotifier(
    std::shared_ptr<IMapNotifier> target, uint32 mapId, uint64 objectGuid,
    Character const &character, MovementInfo const &move,
    PlayerGmAppearanceForUpdates const &gmAppearance,
    GtPlayerStatGameTables const *statGameTables,
    uint32_t nextLevelXpFromWorld,
    std::optional<std::pair<uint32, uint32>> const &healthOverride) {
  if (!target)
    return;
  auto fields = BuildPlayerUpdateFields(objectGuid, character, statGameTables,
                                        nextLevelXpFromWorld, healthOverride);
  MergeGmAppearanceIntoPlayerFields(fields, gmAppearance);
  UpdateData update(mapId);
  update.AddCreateObject(objectGuid, TYPEID_PLAYER, move, fields);
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  target->SendPacket(pkt);
}

} // namespace WorldSessionObjectUpdate
} // namespace Firelands
