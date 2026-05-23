#include <application/combat/PlayerCombatStats.h>

#include <shared/game/UnitCombatStats.h>

namespace Firelands {

bool UsesAgilityForMeleeAttackPower(uint8 classId) {
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

bool UsesBaselineSpellPowerFromIntellect(uint8 classId) {
  switch (classId) {
  case 2:
  case 3:
  case 5:
  case 6:
  case 7:
  case 8:
  case 9:
  case 11:
    return true;
  default:
    return false;
  }
}

UnitCombatStats BuildPlayerCombatStats(Character const &character) {
  UnitCombatStats stats{};
  stats.level = character.GetLevel();
  uint8 const klass = character.GetClass();
  uint32 const agi = character.GetPrimaryStat(1);
  uint32 const str = character.GetPrimaryStat(0);
  uint32 const sta = character.GetPrimaryStat(2);
  uint32 const inte = character.GetPrimaryStat(3);
  uint32 const spi = character.GetPrimaryStat(4);
  uint32 const lv = static_cast<uint32>(stats.level);

  stats.armor = ComputeBaselineArmor(klass, stats.level, agi, str, sta);
  uint32 const mr = lv / 2u + spi / 6u;
  for (uint32 i = 1; i <= 6; ++i)
    stats.resistance[i] = mr;

  stats.attackPower = ComputeBaseMeleeAttackPower(character);

  uint32 spellPower = 0;
  if (UsesBaselineSpellPowerFromIntellect(klass))
    spellPower = inte;
  for (uint32 school = 1; school <= 6; ++school)
    stats.spellDamageDonePos[school] = static_cast<int32>(spellPower);

  return stats;
}

} // namespace Firelands
