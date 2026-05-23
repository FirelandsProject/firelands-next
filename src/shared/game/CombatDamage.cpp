#include <shared/game/CombatDamage.h>

#include <shared/game/SpellSchool.h>

#include <algorithm>
#include <cmath>

namespace Firelands {

namespace {

constexpr float kDefaultDirectSpellCoeff = 0.857f;
constexpr float kDefaultPeriodicSpellCoeff = 0.5f;
constexpr float kBaselineWeaponSpeedSec = 2.0f;

uint32 ApplyMagicResistToDamage(uint32 damage, float averageReduction) {
  if (damage == 0u || averageReduction <= 0.f)
    return damage;
  float const factor = 1.f - std::clamp(averageReduction, 0.f, 0.75f);
  return static_cast<uint32>(
      std::ceil(std::max(static_cast<float>(damage) * factor, 0.f)));
}

} // namespace

uint32 CalcArmorReducedDamage(uint8 attackerLevel, uint32 victimArmor, uint32 damage) {
  if (damage == 0u)
    return 0u;

  float armor = static_cast<float>(victimArmor);
  if (armor < 0.f)
    armor = 0.f;

  float levelModifier = static_cast<float>(attackerLevel > 0 ? attackerLevel : 1);
  if (levelModifier > 59.f)
    levelModifier = levelModifier + 4.5f * (levelModifier - 59.f);

  float damageReduction = 0.1f * armor / (8.5f * levelModifier + 40.f);
  damageReduction /= (1.0f + damageReduction);
  damageReduction = std::clamp(damageReduction, 0.f, 0.75f);

  return static_cast<uint32>(
      std::ceil(std::max(static_cast<float>(damage) * (1.f - damageReduction), 0.f)));
}

float CalcAverageMagicResistReduction(uint8 attackerLevel, uint8 victimLevel,
                                      uint32 victimResistance, bool holySchool) {
  if (holySchool)
    return 0.f;

  float resistance = static_cast<float>(victimResistance);
  int32 const levelDiff =
      static_cast<int32>(victimLevel > 0 ? victimLevel : 1) -
      static_cast<int32>(attackerLevel > 0 ? attackerLevel : 1);
  if (levelDiff > 0)
    resistance += static_cast<float>(levelDiff) * 5.f;
  resistance = std::max(resistance, 0.f);

  uint8 const lvl = victimLevel > 0 ? victimLevel : 1;
  float const constant = (lvl == 83) ? 510.f : static_cast<float>(lvl) * 5.f;
  if (constant <= 0.f)
    return 0.f;
  return resistance / (resistance + constant);
}

uint32 ApplyOutgoingDamageBonuses(uint32 baseDamage, UnitCombatStats const *caster,
                                  uint32 schoolMask, bool periodicTick) {
  if (baseDamage == 0u || caster == nullptr)
    return baseDamage;

  uint8 const school = FirstSchoolFromMask(schoolMask);
  float const coeff =
      periodicTick ? kDefaultPeriodicSpellCoeff : kDefaultDirectSpellCoeff;

  int64 total = static_cast<int64>(baseDamage);
  if (school < 7)
    total += static_cast<int64>(caster->spellDamageDonePos[school] * coeff);

  if (school == 0)
    total += static_cast<int64>(EffectiveAttackPower(*caster) / 14);

  return static_cast<uint32>(std::max<int64>(0, total));
}

uint32 ComputeMeleeSwingRawDamage(UnitCombatStats const &attacker) {
  float const dpsFromAp =
      static_cast<float>(EffectiveAttackPower(attacker)) / 14.0f;
  float const raw = 1.5f + dpsFromAp * kBaselineWeaponSpeedSec;
  return static_cast<uint32>(std::max(1.f, raw));
}

int32 ResolveMitigatedHealthDelta(int32 rawSignedDelta, uint32 schoolMask,
                                  UnitCombatStats const *casterStats,
                                  uint8 casterLevel, UnitCombatStats const *victimStats,
                                  bool periodicTick) {
  if (rawSignedDelta >= 0)
    return rawSignedDelta;

  uint32 damage = ApplyOutgoingDamageBonuses(static_cast<uint32>(-rawSignedDelta),
                                             casterStats, schoolMask, periodicTick);

  uint8 const school = FirstSchoolFromMask(schoolMask);
  uint8 const victimLevel = victimStats ? victimStats->level : 1;
  uint32 const victimArmor = victimStats ? victimStats->armor : 0;
  uint32 const victimResist =
      victimStats && school < 7 ? EffectiveSchoolResistance(*victimStats, school) : 0;

  if (SchoolMaskIsPhysical(schoolMask))
    damage = CalcArmorReducedDamage(casterLevel, victimArmor, damage);

  if (school >= 1 && school <= 6) {
    float const resist = CalcAverageMagicResistReduction(
        casterLevel, victimLevel, victimResist, school == 1);
    damage = ApplyMagicResistToDamage(damage, resist);
  }

  return -static_cast<int32>(damage);
}

uint32 ComputeMeleeSwingDamage(UnitCombatStats const &attacker,
                               UnitCombatStats const &victim) {
  uint32 const raw = ComputeMeleeSwingRawDamage(attacker);
  return CalcArmorReducedDamage(attacker.level, victim.armor, raw);
}

} // namespace Firelands
