#include <shared/game/UnitCombatStats.h>

#include <algorithm>

namespace Firelands {

int32 EffectiveAttackPower(UnitCombatStats const &stats) {
  int64 ap = static_cast<int64>(stats.attackPower) +
             static_cast<int64>(stats.attackPowerModPos) -
             static_cast<int64>(stats.attackPowerModNeg);
  if (stats.attackPowerMultiplier != 0.f)
    ap += static_cast<int64>(static_cast<float>(ap) * stats.attackPowerMultiplier);
  return static_cast<int32>(std::max<int64>(0, ap));
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

namespace {

uint8 MapCreatureUnitClassToPlayerClass(uint8 unitClass) {
  if (unitClass >= 1 && unitClass <= 11)
    return unitClass;
  return 1;
}

} // namespace

UnitCombatStats BuildCreatureCombatStats(uint8 level, uint8 unitClass) {
  UnitCombatStats stats{};
  stats.level = level;
  uint8 const armorClass = MapCreatureUnitClassToPlayerClass(unitClass);
  uint32 const lv = static_cast<uint32>(level);
  uint32 const str = lv * 2u;
  uint32 const agi = lv;
  uint32 const sta = lv * 2u;

  stats.armor = ComputeBaselineArmor(armorClass, level, agi, str, sta);
  uint32 const mr = lv / 2u;
  for (uint32 i = 1; i <= 6; ++i)
    stats.resistance[i] = mr;

  stats.attackPower = static_cast<int32>(lv * 3u + str);
  return stats;
}

void ApplyAuraStatBonusToCombatStats(UnitCombatStats &stats,
                                     int32 attackPowerModPos, int32 attackPowerModNeg,
                                     float attackPowerMultiplier) {
  stats.attackPowerModPos = attackPowerModPos;
  stats.attackPowerModNeg = attackPowerModNeg;
  stats.attackPowerMultiplier = attackPowerMultiplier;
}

uint32 EffectiveSchoolResistance(UnitCombatStats const &stats, uint8 school) {
  if (school >= 7)
    return 0;
  int64 const total = static_cast<int64>(stats.resistance[school]) +
                      static_cast<int64>(stats.resistanceBuffPos[school]) -
                      static_cast<int64>(stats.resistanceBuffNeg[school]);
  return static_cast<uint32>(std::max<int64>(0, total));
}

} // namespace Firelands
